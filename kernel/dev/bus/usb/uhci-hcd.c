/*
 * Ananas UHCI driver
 *
 * The general principle of UHCI is that it has a 'frame list', which contains
 * 1024 pointers; it will start with the first pointer, advance every 1ms to
 * the next and finally return to the first pointer again (the pointer it uses
 * is called the 'frame counter'), and each pointer points to a list of
 * Transfer Descriptors (TD's) and Queue Heads (QH's).
 *
 * The order in each list is: Isochronous, Interrupt, Control and Bulk.
 * Isochronous transfers have timing requirements, and thus we'll want to
 * schedule them in a few frame lists if they need to be filled more often than
 * 1024ms.
 *
 * We can always schedule the other transfer types as the IC (officially called
 * the host controller, or HC) will attempt as many as it can before the time
 * runs out; this allows them to be retried if necessary.
 */
#include <ananas/types.h>
#include <ananas/bus/pci.h>
#include <ananas/bus/usb/descriptor.h>
#include <ananas/bus/usb/core.h>
#include <ananas/x86/io.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/time.h>
#include <machine/param.h>
#include <machine/vm.h>
#include "uhci-hcd.h"
#include "uhci-roothub.h"
#include "uhci-reg.h"

TRACE_SETUP;

static struct UHCI_TD*
uhci_alloc_td(device_t dev)
{
	struct UHCI_HCD_PRIVDATA* privdata = dev->privdata;

	/* XXX We need some kind of locking here */
	struct UHCI_TD* td = privdata->uhci_td_freelist;
	KASSERT(td != NULL, "out of td's");
	privdata->uhci_td_freelist = (void*)td->td_linkptr;
	return td;
}

/* Allocates a QH; will be empty */
static struct UHCI_QH*
uhci_alloc_qh(device_t dev)
{
	struct UHCI_HCD_PRIVDATA* privdata = dev->privdata;

	/* XXX We need some kind of locking here */

	struct UHCI_QH* qh = privdata->uhci_qh_freelist;
	KASSERT(qh != NULL, "out of qh's");
	privdata->uhci_qh_freelist = (void*)qh->qh_elementptr;

	/* Set up the qh: we want it to be empty */
	qh->qh_elementptr = TO_REG32(QH_PTR_T);
	return qh;
}

#define UHCI_GET_PTR(x) \
	(((addr_t)x) & 0xfffffff0)

static void
uhci_dump_td(struct UHCI_TD* td)
{
	kprintf("td %p => linkptr 0x%x status 0x%x [",
		td, td->td_linkptr, td->td_status);
	if (td->td_status & TD_STATUS_SPD)        kprintf("spd ");
	if (td->td_status & TD_STATUS_LS)         kprintf("ls ");
	if (td->td_status & TD_STATUS_IOS)        kprintf("ios ");
	if (td->td_status & TD_STATUS_IOC)        kprintf("ioc ");
	if (td->td_status & TD_STATUS_ACTIVE)     kprintf("act ");
	if (td->td_status & TD_STATUS_STALLED)    kprintf("stl ");
	if (td->td_status & TD_STATUS_DATABUFERR) kprintf("dbe ");
	if (td->td_status & TD_STATUS_BABBLE)     kprintf("bab ");
	if (td->td_status & TD_STATUS_NAK)        kprintf("nak ");
	if (td->td_status & TD_STATUS_CRCTOERR)   kprintf("crc ");
	if (td->td_status & TD_STATUS_BITSTUFF)   kprintf("bst ");
	kprintf("] token 0x%x (data%u,maxlen=%u,endpt=%u,addr=%u)) buffer 0x%x\n",
	 td->td_token, (td->td_token & TD_TOKEN_DATA) ? 1 : 0, (td->td_token >> 21) + 1,
	 (td->td_token >> 15) & 0xf, (td->td_token >> 8) & 0x7f,
	 td->td_buffer);
	if (td->td_linkptr & QH_PTR_T)
		return;
	uhci_dump_td((struct UHCI_TD*)UHCI_GET_PTR(PTOKV(td->td_linkptr)));
}

static void
uhci_dump_qh(struct UHCI_QH* qh)
{
	kprintf("qh %p => vertptr 0x%x horizptr 0x%x\n", qh, qh->qh_headptr, qh->qh_elementptr);
	if ((qh->qh_elementptr & QH_PTR_T) == 0) {
		KASSERT((qh->qh_elementptr & QH_PTR_QH) == 0, "nesting qh in qh?");
		uhci_dump_td((struct UHCI_TD*)UHCI_GET_PTR(PTOKV(qh->qh_elementptr)));
	}
	if (qh->qh_headptr & QH_PTR_T)
		return;
	if (qh->qh_headptr & QH_PTR_QH)
		uhci_dump_qh((struct UHCI_QH*)UHCI_GET_PTR(PTOKV(qh->qh_headptr)));
	else
		uhci_dump_td((struct UHCI_TD*)UHCI_GET_PTR(PTOKV(qh->qh_headptr)));
}

static void
uhci_dump(device_t dev)
{
	struct UHCI_HCD_PRIVDATA* privdata = dev->privdata;

	int frnum     = inw(privdata->uhci_io + UHCI_REG_FRNUM) & 0x3ff;
	addr_t flbase = inl(privdata->uhci_io + UHCI_REG_FLBASEADD) & 0xfffff000;
	kprintf("uhci dump\n"); 
	kprintf(" cmd 0x%x", inw(privdata->uhci_io + UHCI_REG_USBCMD));
	kprintf(" status 0x%x", inw(privdata->uhci_io + UHCI_REG_USBSTS));
	kprintf(" intr 0x%x", inw(privdata->uhci_io + UHCI_REG_USBINTR));
	kprintf(" frnum %u",   frnum);
	kprintf(" flbase %p",   flbase);
	kprintf(" sof %u",   inw(privdata->uhci_io + UHCI_REG_SOF));
	kprintf(" portsc1 0x%x", inw(privdata->uhci_io + UHCI_REG_PORTSC1));
	kprintf(" portsc2 0x%x", inw(privdata->uhci_io + UHCI_REG_PORTSC2));
	KASSERT((addr_t)privdata->uhci_framelist == PTOKV(flbase), "framelist %p not in framelist base register %p?", privdata->uhci_framelist, PTOKV(flbase));
	uint32_t fl_ptr = *(uint32_t*)(PTOKV(flbase) + frnum * sizeof(uint32_t));
	kprintf(" flptr 0x%x\n", fl_ptr);
	KASSERT(((addr_t)fl_ptr & QH_PTR_QH) != 0, "fl_ptr %p: not a qh at the root?", fl_ptr);
	uhci_dump_qh((struct UHCI_QH*)UHCI_GET_PTR(fl_ptr));
}

static void	
uhci_free_td(device_t dev, struct UHCI_TD* td)
{
	struct UHCI_HCD_PRIVDATA* privdata = dev->privdata;

	/* XXX Lock */
	td->td_linkptr = (addr_t)privdata->uhci_td_freelist;
	privdata->uhci_td_freelist = td;
}

static void	
uhci_free_tdchain(device_t dev, addr_t addr, int* length, int* error)
{
	*length = 0; *error = 0;
	while ((addr & QH_PTR_T) == 0) {
		struct UHCI_TD* td = (struct UHCI_TD*)UHCI_GET_PTR(PTOKV(addr));
		addr_t next_td = td->td_linkptr;
		int l = TD_STATUS_ACTUALLEN(td->td_status);
		if (l != TD_ACTUALLEN_NONE)
			*length += l;
		if (td->td_status & (TD_STATUS_STALLED | TD_STATUS_DATABUFERR | TD_STATUS_BABBLE | TD_STATUS_NAK | TD_STATUS_CRCTOERR | TD_STATUS_BITSTUFF))
			(*error)++;
		uhci_free_td(dev, td);
		addr = next_td;
	}
}

static void
uhci_irq(device_t dev)
{
	struct UHCI_HCD_PRIVDATA* privdata = dev->privdata;
	int stat = inw(privdata->uhci_io + UHCI_REG_USBSTS);
	outw(privdata->uhci_io + UHCI_REG_USBSTS, stat);

//	kprintf("uhci_irq: stat=%x\n", stat);

	if (stat & UHCI_USBSTS_ERR) {
		device_printf(dev, "USB ERROR received");
	}

	if (stat & UHCI_USBSTS_HCHALTED) {
		device_printf(dev, "ERROR: Host Controller Halted!");
		uhci_dump(dev);
	}

	if (stat & UHCI_USBSTS_HCPE) {
		device_printf(dev, "ERROR: Host Process Error");
	}

	if (stat & UHCI_USBSTS_HSE) {
		device_printf(dev, "ERROR: Host System Error");
	}

	if (stat & UHCI_USBSTS_USBINT) {
		/*
		 * We got an interrupt because something was completed; but we have no idea
		 * what it was. We'll have to traverse the scheduled items and wake anything
		 * up that finished.
		 */
		if (!DQUEUE_EMPTY(&privdata->uhci_scheduled_items)) {
			DQUEUE_FOREACH_SAFE(&privdata->uhci_scheduled_items, si, struct UHCI_SCHEDULED_ITEM) {
				/*
				 * Transfers are scheduled in such a way that we can use the first TD to
				 * determine whether the transfer went OK (as only the final TD will have the
				 * interrupt-on-completion flag enabled)
				 */
				if (si->si_td->td_status & TD_STATUS_ACTIVE)
					continue;

				/* First of all, remove the scheduled item - this orphanages the TD's */
				DQUEUE_REMOVE(&privdata->uhci_scheduled_items, si);

				/* Throw away the TDs; they served their purpose. This also inspects them */
				int error;
				uhci_free_tdchain(dev, KVTOP((addr_t)si->si_td), &si->si_xfer->xfer_result_length, &error);

				/* If something failed, mark the transfer as such */
				if (error)
					si->si_xfer->xfer_flags |= TRANSFER_FLAG_ERROR;

				/* Finally, give hand the transfer back to the USB stack */
				usb_completed_transfer(si->si_xfer);
			}
		}
	}
}

static struct UHCI_TD*
uhci_create_data_tds(device_t dev, void* data, int size, int max_packet_size, int token, int ls, int token_addr, struct UHCI_TD* link_td, struct UHCI_TD** last_td)
{
	int num_datapkts = (size + max_packet_size - 1) / max_packet_size;
	addr_t cur_data_ptr = (addr_t)data + size;
	/*
	 * Data packets are alternating, in DATA1/0/1/0/... fashion; note that we build them in
	 * reverse.
	 */
	int data_token = num_datapkts % 2;
	for (int i = 0; i < num_datapkts; i++) {
		struct UHCI_TD* td_data = uhci_alloc_td(dev);
		int xfer_chunk_len = (i == 0) ? (size % max_packet_size) : max_packet_size;
		if (size == max_packet_size) /* XXX ugly fix because max_packet_size % max_packet_size = 0 */
			xfer_chunk_len = max_packet_size;

		if (i == 0 && last_td != NULL)
			*last_td = td_data;
		
		td_data->td_linkptr = TO_REG32(TD_LINKPTR_VF | KVTOP((addr_t)link_td));
		td_data->td_status = TO_REG32(ls | TD_STATUS_ACTIVE | TD_STATUS_INTONERR(3));
		td_data->td_token = TO_REG32(token_addr | TD_TOKEN_MAXLEN(xfer_chunk_len) | TD_TOKEN_PID(token) | (data_token ? TD_TOKEN_DATA : 0));
		td_data->td_buffer = KVTOP(cur_data_ptr - xfer_chunk_len);
		data_token ^= 1;
		link_td = td_data;
		cur_data_ptr -= xfer_chunk_len;
	}
	KASSERT(cur_data_ptr == (addr_t)data, "bug");
	return link_td;
}

static errorcode_t
uhci_ctrl_schedule_xfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct UHCI_HCD_PRIVDATA* privdata = dev->privdata;
	struct UHCI_DEV_PRIVDATA* hcd_privdata = xfer->xfer_device->usb_hcd_privdata;
	int ls = (hcd_privdata->dev_flags & UHCI_DEV_FLAG_LOWSPEED) ? TD_STATUS_LS : 0;
	void* next_setup_ptr;
	uint32_t token_addr = TD_TOKEN_ENDPOINT(xfer->xfer_endpoint) | TD_TOKEN_ADDRESS(xfer->xfer_address);
	int isread = xfer->xfer_flags & TRANSFER_FLAG_READ;

	/*
	 * A control request consists of the following stages, in order:
	 *
	 *  SETUP -> (DATA) -> HANDSHAKE
	 *
	 * DATA is optional and need only be sent if there is data to send/receive.
	 * As the pointer chain is a linked, we create them in reverse order so that
	 * we can get the links correct the first time.
	 *
	 * Note that the HD will add the DATAx packets as necessary; this is contained
	 * within the TD.
	 */

	/* Create the HANDSHAKE packet */
	struct UHCI_TD* td_handshake = uhci_alloc_td(dev);
	td_handshake->td_linkptr = TO_REG32(TD_LINKPTR_T);
	td_handshake->td_status = TO_REG32(ls | TD_STATUS_IOC | TD_STATUS_ACTIVE | TD_STATUS_INTONERR(3));
	td_handshake->td_token = TO_REG32(token_addr | TD_TOKEN_PID(isread ? TD_PID_OUT : TD_PID_IN) | TD_TOKEN_DATA);
	td_handshake->td_buffer = 0;

	/*
	 * Create the DATA stage packets, if any - note that we make them in *reverse*
	 * order so that we can hook them together immediately (order is very
	 * important as they will be filled from first to last)
	 */
	if (xfer->xfer_flags & TRANSFER_FLAG_DATA) {
		next_setup_ptr = uhci_create_data_tds(dev, xfer->xfer_data, xfer->xfer_length, xfer->xfer_device->usb_max_packet_sz0, isread ? TD_PID_IN : TD_PID_OUT, ls, token_addr, td_handshake, NULL);
	} else
		next_setup_ptr = td_handshake;

	/* Create the SETUP stage packet (SETUP + DATA0) */
	struct UHCI_TD* td_setup = uhci_alloc_td(dev);
	td_setup->td_linkptr = TO_REG32(TD_LINKPTR_VF | KVTOP((addr_t)next_setup_ptr));
	td_setup->td_status = TO_REG32(ls | TD_STATUS_ACTIVE | TD_STATUS_INTONERR(3));
	td_setup->td_token = TO_REG32(TD_TOKEN_MAXLEN(sizeof(struct USB_CONTROL_REQUEST)) | token_addr | TD_TOKEN_PID(TD_PID_SETUP));
	td_setup->td_buffer = KVTOP((addr_t)&xfer->xfer_control_req);

	/* All set - need to hook the packet up XXX should be at the end */
	//td_setup->td_linkptr = privdata->uhci_qh_control->qh_elementptr;

#if 0
	uhci_dump_td(td_setup);
	while(1);
#endif

	/* Schedule an item; this will cause the IRQ to handle our request - XXX needs lock */
	struct UHCI_SCHEDULED_ITEM* si = kmalloc(sizeof *si);
	si->si_td = td_setup;
	si->si_xfer = xfer;
	DQUEUE_ADD_TAIL(&privdata->uhci_scheduled_items, si);

	/* Finally, hand the chain to the HD; it's ready to be transmitted */
	//kprintf("uhci: hooked %p to control ptr chain\n", td_setup);
	privdata->uhci_qh_control->qh_elementptr = KVTOP((addr_t)td_setup);
	return ANANAS_ERROR_OK;
}

static errorcode_t
uhci_interrupt_schedule_xfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct UHCI_HCD_PRIVDATA* privdata = dev->privdata;
	struct UHCI_DEV_PRIVDATA* hcd_privdata = xfer->xfer_device->usb_hcd_privdata;
	int ls = (hcd_privdata->dev_flags & UHCI_DEV_FLAG_LOWSPEED) ? TD_STATUS_LS : 0;
	uint32_t token_addr = TD_TOKEN_ENDPOINT(xfer->xfer_endpoint) | TD_TOKEN_ADDRESS(xfer->xfer_address);
	int isread = xfer->xfer_flags & TRANSFER_FLAG_READ;

	struct UHCI_TD* last_td;
	struct UHCI_TD* td_chain = uhci_create_data_tds(dev, xfer->xfer_data, xfer->xfer_length, xfer->xfer_device->usb_max_packet_sz0, isread ? TD_PID_IN : TD_PID_OUT, ls, token_addr, (void*)TD_LINKPTR_T, &last_td);

	last_td->td_status |= TD_STATUS_IOC;
	td_chain->td_token &= ~TD_TOKEN_DATA; /* XXX */

	/* Schedule an item; this will cause the IRQ to handle our request - XXX needs lock */
	struct UHCI_SCHEDULED_ITEM* si = kmalloc(sizeof *si);
	si->si_td = td_chain;
	si->si_xfer = xfer;
	DQUEUE_ADD_TAIL(&privdata->uhci_scheduled_items, si);

	/* Finally, hand the chain to the HD; it's ready to be transmitted */
	privdata->uhci_qh_interrupt->qh_elementptr = KVTOP((addr_t)td_chain);
	return ANANAS_ERROR_OK;
}

static errorcode_t
uhci_schedule_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	errorcode_t err = ANANAS_ERROR_OK;

	switch(xfer->xfer_type) {
		case TRANSFER_TYPE_CONTROL:
			err = uhci_ctrl_schedule_xfer(dev, xfer);
			break;
		case TRANSFER_TYPE_INTERRUPT:
			err = uhci_interrupt_schedule_xfer(dev, xfer);
			break;
		default:
			panic("unsupported transfer type %u", xfer->xfer_type);
	}

	return err;
}

static errorcode_t
uhci_attach(device_t dev)
{
	/*
	 * Disable legacy PS/2 support; we do not want it, and it can only interfere
	 * with our operations.
	 */
	pci_write_cfg(dev, UHCI_PCI_LEGSUPP, UHCI_LEGSUPP_PIRQEN, 2);

  void* res_io = device_alloc_resource(dev, RESTYPE_IO, 16);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	struct UHCI_HCD_PRIVDATA* privdata = kmalloc(sizeof *privdata);
	memset(privdata, 0, sizeof *privdata);
	dev->privdata = privdata;
	privdata->uhci_io = (uint32_t)res_io;
	DQUEUE_INIT(&privdata->uhci_scheduled_items);

	/* Disable interrupts; we don't want the messing along */
	outw(privdata->uhci_io + UHCI_REG_USBINTR, 0);

	/* Set up a TD freelist; we don't want to alloc them on the fly */
	privdata->uhci_td_freelist = kmalloc(UHCI_NUM_TDS * sizeof(struct UHCI_TD));
	KASSERT((((addr_t)privdata->uhci_td_freelist) & 0xf) == 0, "td freelist misaligned");
	memset(privdata->uhci_td_freelist, 0, UHCI_NUM_TDS * sizeof(struct UHCI_TD));
	for (int i = 0; i < UHCI_NUM_TDS; i++) {
		privdata->uhci_td_freelist[i].td_linkptr = TO_REG32((addr_t)&privdata->uhci_td_freelist[i + 1]);
	}

	/* Set up a QH freelist as well */
	privdata->uhci_qh_freelist = kmalloc(UHCI_NUM_QHS * sizeof(struct UHCI_QH));
	KASSERT((((addr_t)privdata->uhci_qh_freelist) & 0xf) == 0, "qh freelist misaligned");
	memset(privdata->uhci_qh_freelist, 0, UHCI_NUM_QHS * sizeof(struct UHCI_QH));
	for (int i = 0; i < UHCI_NUM_QHS; i++) {
		privdata->uhci_qh_freelist[i].qh_elementptr = TO_REG32((addr_t)&privdata->uhci_qh_freelist[i + 1]);
	}

	/*
	 * Allocate our interrupt, control and bulk queue's. These will be used for
	 * every frame like the following:
	 *
	 * (framelist) -> (isochronous td) -> interrupt -> control -> bulk -> (end)
	 *
	 * UHCI spec, 3.4.2 seems to indicate we'll need an end TD to terminate the
	 * transfer list; this shouldn't be necessary if we just set the final QH's
	 * next pointer to be terminating - but old PIIX controllers seem to require
	 * it, so we just add it
	 */
	privdata->uhci_qh_interrupt = uhci_alloc_qh(dev);
	privdata->uhci_qh_control   = uhci_alloc_qh(dev);
	privdata->uhci_qh_bulk      = uhci_alloc_qh(dev);
	struct UHCI_TD* last_td     = uhci_alloc_td(dev);
	last_td->td_linkptr = TO_REG32(TD_LINKPTR_T);
	privdata->uhci_qh_interrupt->qh_headptr = TO_REG32(QH_PTR_QH | KVTOP((addr_t)privdata->uhci_qh_control));
	privdata->uhci_qh_control->qh_headptr   = TO_REG32(QH_PTR_QH | KVTOP((addr_t)privdata->uhci_qh_bulk));
	privdata->uhci_qh_bulk->qh_headptr      = TO_REG32(KVTOP((addr_t)last_td));

	/*
	 * Set up the frame list; we add a dummy TD for isochronous data and then the
	 * QH's set up above
	 */
	for (int i = 0; i < UHCI_FRAMELIST_LEN; i++) {
#if 0
		struct UHCI_TD* framelist_td = uhci_alloc_td(dev);
		framelist_td->td_linkptr = TO_REG32(TD_LINKPTR_QH | (addr_t)privdata->uhci_qh_interrupt);
#endif
		//privdata->uhci_framelist[i] = TO_REG32(TD_LINKPTR_QH | (addr_t)framelist_td);
		privdata->uhci_framelist[i] = TO_REG32(TD_LINKPTR_QH | KVTOP((addr_t)privdata->uhci_qh_interrupt));
		//kprintf("fl[%u] = %p\n", i, privdata->uhci_framelist[i]);
	}
	KASSERT((((addr_t)privdata->uhci_framelist) & 0x3ff) == 0, "framelist %p misaligned", (addr_t)privdata->uhci_framelist);

	/*
	 * Grab a copy of the SOF modify register; we have no idea what it should be,
	 * so we just rely on the BIOS doing the right thing.
	 */
	privdata->uhci_sof_modify = inw(privdata->uhci_io + UHCI_REG_SOF);

	/* Reset the host controller */
	outw(privdata->uhci_io + UHCI_REG_USBCMD, UHCI_USBCMD_GRESET);
	delay(10);
	outw(privdata->uhci_io + UHCI_REG_USBCMD, 0);
	delay(1);

	/* Now issue a host controller reset and wait until it is done */
	outw(privdata->uhci_io + UHCI_REG_USBCMD, UHCI_USBCMD_HCRESET);
	int timeout = 50000;
	while (timeout > 0) {
		if ((inw(privdata->uhci_io + UHCI_REG_USBCMD) & UHCI_USBCMD_HCRESET) == 0)
			break;
		timeout--;
	}
	if (timeout == 0)
		device_printf(dev, "warning: no response on reset");

	/*
	 * Program the USB frame number, start of frame and frame list address base
	 * registers.
	 */
	outw(privdata->uhci_io + UHCI_REG_FRNUM, 0);
	outw(privdata->uhci_io + UHCI_REG_SOF, privdata->uhci_sof_modify);
	outl(privdata->uhci_io + UHCI_REG_FLBASEADD, TO_REG32(KVTOP((addr_t)privdata->uhci_framelist)));

	/* Tell the USB controller to start pumping frames */
	outw(privdata->uhci_io + UHCI_REG_USBCMD, UHCI_USBCMD_MAXP | UHCI_USBCMD_RS);
	delay(10);
	if (inw(privdata->uhci_io + UHCI_REG_USBSTS) & UHCI_USBSTS_HCHALTED) {
		device_printf(dev, "controller does not start");
		/* XXX Free resources */
		return ANANAS_ERROR(NO_DEVICE);
	}

	/* Hook up our interrupt handler and get it going */
	if (!irq_register((int)res_irq, dev, uhci_irq))
		return ANANAS_ERROR(NO_RESOURCE);
	outw(privdata->uhci_io + UHCI_REG_USBINTR, UHCI_USBINTR_SPI | UHCI_USBINTR_IOC | UHCI_USBINTR_RI | UHCI_USBINTR_TOCRC);
	delay(10);

	/*
	 * We are all set up and good to go; attach the root hub. It is used to
	 * actually detect the devices.
	 *
	 * Note that there doesn't seem to be a way to ask the HC how much ports it
	 * has; we try to take advantage of the fact that bit 7 is always set, so if
	 * this is true and not all other bits are set as well, we assume there's a
	 * port there...
	 */
	int num_ports = 0;
	while (1) {
		int stat = inw(privdata->uhci_io + UHCI_REG_PORTSC1 + num_ports * 2);
		if ((stat & UHCI_PORTSC_ALWAYS1) == 0 || (stat == 0xffff))
			break;
		num_ports++;
	}
	uhci_hub_create(dev, num_ports);

	return ANANAS_ERROR_NONE;
}

void*
uhci_device_init_privdata(int ls)
{
	struct UHCI_DEV_PRIVDATA* privdata = kmalloc(sizeof *privdata);
	memset(privdata, 0, sizeof *privdata);
	privdata->dev_flags = ls;
	return privdata;
}

static int
uhci_match_dev(device_t dev)
{
	struct RESOURCE* class_res  = device_get_resource(dev, RESTYPE_PCI_CLASSREV, 0);
	if (class_res == NULL) /* XXX it's a bug if this happens */
		return ANANAS_ERROR(NO_RESOURCE);
	uint32_t classrev = class_res->base;

	/* Generic UHCI USB device */
	if (PCI_CLASS(classrev) == PCI_CLASS_SERIAL && PCI_SUBCLASS(classrev) == PCI_SUBCLASS_USB && PCI_PROGINT(classrev) == 0)
		return ANANAS_ERROR_OK;
	return ANANAS_ERROR(NO_RESOURCE);
}

static errorcode_t
uhci_probe(device_t dev)
{
	/* XXX This is a crude hack to ensure we only use one UHCI device for now */
	if (dev->unit > 0) return ANANAS_ERROR(NO_RESOURCE);
	return uhci_match_dev(dev);
}

struct DRIVER drv_uhci = {
	.name					= "uhci",
	.drv_probe		= uhci_probe,
	.drv_attach		= uhci_attach,
	.drv_usb_xfer = uhci_schedule_transfer
};

DRIVER_PROBE(uhci)
DRIVER_PROBE_BUS(pcibus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
