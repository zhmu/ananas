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
#include <ananas/dma.h>
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
#include "usb-device.h"
#include "usb-transfer.h"

TRACE_SETUP;

struct UHCI_HCD_TD {
	struct UHCI_TD td_td;
	dma_buf_t td_buf;
	LIST_FIELDS(struct UHCI_HCD_TD);
};

struct UHCI_HCD_QH {
	struct UHCI_QH qh_qh;
	struct UHCI_HCD_TD* qh_first_td;
	struct UHCI_HCD_QH* qh_next_qh;
	dma_buf_t qh_buf;
	LIST_FIELDS(struct UHCI_HCD_QH);
};

struct UHCI_SCHEDULED_ITEM {
	struct UHCI_HCD_TD* si_td;
	struct USB_TRANSFER* si_xfer;
	LIST_FIELDS(struct UHCI_SCHEDULED_ITEM);
};

static struct UHCI_HCD_TD*
uhci_alloc_td(device_t dev)
{
	dma_buf_t buf;
	errorcode_t err = dma_buf_alloc(dev->dma_tag, sizeof(struct UHCI_HCD_TD), &buf);
	if (err != ANANAS_ERROR_NONE)
		return NULL;

	struct UHCI_HCD_TD* td = dma_buf_get_segment(buf, 0)->s_virt;
	memset(td, 0, sizeof(struct UHCI_HCD_TD));
	td->td_buf = buf;
	return td;
}

/* Allocates a QH; will be empty */
static struct UHCI_HCD_QH*
uhci_alloc_qh(device_t dev)
{
	dma_buf_t buf;
	errorcode_t err = dma_buf_alloc(dev->dma_tag, sizeof(struct UHCI_HCD_QH), &buf);
	if (err != ANANAS_ERROR_NONE)
		return NULL;

	struct UHCI_HCD_QH* qh = dma_buf_get_segment(buf, 0)->s_virt;
	qh->qh_buf = buf;
	qh->qh_first_td = NULL;
	qh->qh_next_qh = NULL;

	/* Initialize the QH; we'll clear all links */
	memset(&qh->qh_qh, 0, sizeof(struct UHCI_QH));
	qh->qh_qh.qh_headptr = TO_REG32(QH_PTR_T);
	qh->qh_qh.qh_elementptr = TO_REG32(QH_PTR_T);
	return qh;
}

static inline addr_t
uhci_td_get_phys(struct UHCI_HCD_TD* td)
{
	if (td == NULL)
		return TD_LINKPTR_T;
	return dma_buf_get_segment(td->td_buf, 0)->s_phys;
}

static inline addr_t
uhci_qh_get_phys(struct UHCI_HCD_QH* qh)
{
	return dma_buf_get_segment(qh->qh_buf, 0)->s_phys;
}

static void
uhci_dump_td(struct UHCI_HCD_TD* tdd)
{
	if (tdd == NULL)
		return;

	struct UHCI_TD* td = &tdd->td_td;
	uint32_t td_status = td->td_status;
	kprintf("td [hcd %p td %p] => linkptr [hcd %p td %x] status 0x%x [%c%c%c%c%c%c%c%c%c%c%c]",
	 td, &tdd->td_td,
	 LIST_NEXT(tdd), td->td_linkptr,
	 td->td_status,
	 (td_status & TD_STATUS_SPD) ? 'S' : '.',
	 (td_status & TD_STATUS_LS) ? 'L' : '.',
	 (td_status & TD_STATUS_IOS) ? 'I' : '.',
	 (td_status & TD_STATUS_IOC) ? 'O' : '.',
	 (td_status & TD_STATUS_ACTIVE) ? 'A' : '.',
	 (td_status & TD_STATUS_STALLED) ? 'T' : '.',
	 (td_status & TD_STATUS_DATABUFERR) ? 'D' : '.',
	 (td_status & TD_STATUS_BABBLE) ? 'B' : '.',
	 (td_status & TD_STATUS_NAK) ? 'N' : '.',
	 (td_status & TD_STATUS_CRCTOERR) ? 'C' : '.',
	 (td_status & TD_STATUS_BITSTUFF) ? 'Z' : '.');
	kprintf(" token 0x%x (data%u,maxlen=%u,endpt=%u,addr=%u)) buffer 0x%x\n",
	 td->td_token, (td->td_token & TD_TOKEN_DATA) ? 1 : 0, (td->td_token >> 21) + 1,
	 (td->td_token >> 15) & 0xf, (td->td_token >> 8) & 0x7f,
	 td->td_buffer);
	if (td->td_linkptr & QH_PTR_T)
		return;
	uhci_dump_td(LIST_NEXT(tdd));
}

static void
uhci_dump_qh(struct UHCI_HCD_QH* qhh)
{
	if (qhh == NULL)
		return;

	struct UHCI_QH* qh = &qhh->qh_qh;
	kprintf("qh [hcd %p qh %p]  => headptr [ hcd %p qh 0x%x ] elemptr [ hcd %p qh %p]\n",
	 qhh, qh,
	 qhh->qh_next_qh, qh->qh_headptr,
	 qhh->qh_first_td, qh->qh_elementptr);
	if ((qh->qh_elementptr & QH_PTR_T) == 0) {
		KASSERT((qh->qh_elementptr & QH_PTR_QH) == 0, "nesting qh in qh?");
		uhci_dump_td(qhh->qh_first_td);
	}
	if (qh->qh_headptr & QH_PTR_T)
		return;
	if (qh->qh_headptr & QH_PTR_QH)
		uhci_dump_qh(qhh->qh_next_qh);
	else
		uhci_dump_td(qhh->qh_first_td); // XXX This is wrong
}

static void
uhci_dump(device_t dev)
{
	struct UHCI_PRIVDATA* privdata = dev->privdata;

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
	// XXX we should look up the UHCI_HCD_QH to use
	for (int n = 0; n < UHCI_NUM_INTERRUPT_QH; n++) {
		kprintf("> %d ms\n", 1 << n);
		uhci_dump_qh(privdata->uhci_qh_interrupt[n]);
	}
}

#if 0
static void	
uhci_free_td(device_t dev, struct UHCI_HCD_TD* td)
{
	dma_buf_free(td->td_buf);
}
#endif

static void	
uhci_free_qh(device_t dev, struct UHCI_HCD_QH* qh)
{
	dma_buf_free(qh->qh_buf);
}

/* Calculates total number of bytes transferred and checks errors */
static int
uhci_inspect_chain(struct UHCI_HCD_TD* td, int* length)
{
	int errors = 0;
	*length = 0;
	for (/* nothing */; td != NULL; td = LIST_NEXT(td)) {
		int len = TD_STATUS_ACTUALLEN(td->td_td.td_status);
		if (len != TD_ACTUALLEN_NONE)
			*length += len;
		if (td->td_td.td_status & (TD_STATUS_STALLED | TD_STATUS_DATABUFERR | TD_STATUS_BABBLE | TD_STATUS_NAK | TD_STATUS_CRCTOERR | TD_STATUS_BITSTUFF))
			errors++;
	}
	return errors;
}

static irqresult_t
uhci_irq(device_t dev, void* context)
{
	struct UHCI_PRIVDATA* privdata = dev->privdata;
	int stat = inw(privdata->uhci_io + UHCI_REG_USBSTS);
	outw(privdata->uhci_io + UHCI_REG_USBSTS, stat);

//	kprintf("uhci_irq: stat=%x\n", stat);

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
		LIST_FOREACH_SAFE(&privdata->uhci_scheduled_items, si, struct UHCI_SCHEDULED_ITEM) {
			/*
			 * Transfers are scheduled in such a way that we can use the first TD to
			 * determine whether the transfer went OK (as only the final TD will have the
			 * interrupt-on-completion flag enabled)
			 */
			if (si->si_td->td_td.td_status & TD_STATUS_ACTIVE)
				continue;

			/* First of all, remove the scheduled item - this orphanages the TD's */
			LIST_REMOVE(&privdata->uhci_scheduled_items, si);

			/* Walk through the chain to calculate the length and see if something gave an error */
			if (uhci_inspect_chain(si->si_td, &si->si_xfer->xfer_result_length))
				si->si_xfer->xfer_flags |= TRANSFER_FLAG_ERROR;

			/* Finally, give hand the transfer back to the USB stack */
			usbtransfer_complete(si->si_xfer);
		}
	}

	return IRQ_RESULT_PROCESSED;
}

/* Creates a TD-chain for [size] bytes of [data] and links it to [link_td] */
static struct UHCI_HCD_TD*
uhci_create_data_tds(device_t dev, addr_t data, size_t size, int max_packet_size, int token, int ls, int token_addr, struct UHCI_HCD_TD* link_td, struct UHCI_HCD_TD** last_td)
{
	int num_datapkts = (size + max_packet_size - 1) / max_packet_size;
	addr_t cur_data_ptr = data + size;

	/*
	 * Data packets are alternating, in DATA1/0/1/0/... fashion; note that we build them in
	 * reverse.
	 */
	int data_token = num_datapkts % 2;
	for (int i = 0; i < num_datapkts; i++) {
		int xfer_chunk_len = (i == 0) ? (size % max_packet_size) : max_packet_size;
		if (size == max_packet_size) /* XXX ugly fix because max_packet_size % max_packet_size = 0 */
			xfer_chunk_len = max_packet_size;

		struct UHCI_HCD_TD* td_data = uhci_alloc_td(dev);
		if (i == 0 && last_td != NULL)
			*last_td = td_data;

		td_data->td_td.td_linkptr = TO_REG32(TD_LINKPTR_VF | uhci_td_get_phys(link_td));
		td_data->td_td.td_status = TO_REG32(ls | TD_STATUS_ACTIVE | TD_STATUS_INTONERR(3));
		td_data->td_td.td_token = TO_REG32(token_addr | TD_TOKEN_MAXLEN(xfer_chunk_len) | TD_TOKEN_PID(token) | (data_token ? TD_TOKEN_DATA : 0));
		td_data->td_td.td_buffer = cur_data_ptr - xfer_chunk_len;
		LIST_NEXT(td_data) = link_td;

		data_token ^= 1;
		link_td = td_data;
		cur_data_ptr -= xfer_chunk_len;
	}
	KASSERT(cur_data_ptr == data, "bug");
	return link_td;
}

static errorcode_t
uhci_setup_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	/*
	 * Create a Queue Head for the transfer; we'll hook all TD's to this QH, but we'll
	 * only create the TD's in uhci_schedule_transfer() as we won't know the length
	 * beforehand.
	 */
	struct UHCI_HCD_QH* qh = uhci_alloc_qh(dev);
	xfer->xfer_hcd = qh;
	return ANANAS_ERROR_NONE;
}

static errorcode_t
uhci_teardown_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	if (xfer->xfer_hcd != NULL) {
		/* XXX We should ensure it's no longer in use */
		uhci_free_qh(dev, xfer->xfer_hcd);
	}

	xfer->xfer_hcd = NULL;
	return ANANAS_ERROR_NONE;
}

static errorcode_t
uhci_cancel_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	mutex_assert(&xfer->xfer_device->usb_mutex, MTX_LOCKED);

	if (xfer->xfer_flags & TRANSFER_FLAG_PENDING) {
		xfer->xfer_flags &= ~TRANSFER_FLAG_PENDING;
		LIST_REMOVE_IP(&xfer->xfer_device->usb_transfers, pending, xfer);
	}

	kprintf("uhci_cancel_transfer(): TODO\n");
	return ANANAS_ERROR_NONE;
}

static errorcode_t
uhci_ctrl_schedule_xfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct UHCI_PRIVDATA* p = dev->privdata;
	struct UHCI_DEV_PRIVDATA* hcd_privdata = xfer->xfer_device->usb_hcd_privdata;
	int ls = (hcd_privdata->dev_flags & UHCI_DEV_FLAG_LOWSPEED) ? TD_STATUS_LS : 0;
	uint32_t token_addr = TD_TOKEN_ENDPOINT(xfer->xfer_endpoint) | TD_TOKEN_ADDRESS(xfer->xfer_address);
	int isread = xfer->xfer_flags & TRANSFER_FLAG_READ;

	kprintf("%s: xfer flags %x\n", __func__, xfer->xfer_flags);

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
	struct UHCI_HCD_TD* td_handshake = uhci_alloc_td(dev);
	td_handshake->td_td.td_linkptr = TO_REG32(TD_LINKPTR_T);
	td_handshake->td_td.td_status = TO_REG32(ls | TD_STATUS_IOC | TD_STATUS_ACTIVE | TD_STATUS_INTONERR(3));
	td_handshake->td_td.td_token = TO_REG32(token_addr | TD_TOKEN_PID(isread ? TD_PID_OUT : TD_PID_IN) | TD_TOKEN_DATA);
	td_handshake->td_td.td_buffer = 0;

	/*
	 * Create the DATA stage packets, if any - note that we make them in *reverse*
	 * order so that we can hook them together immediately (order is very
	 * important as they will be filled from first to last)
	 */
	struct UHCI_HCD_TD* next_setup_ptr;
	if (xfer->xfer_flags & TRANSFER_FLAG_DATA) {
		next_setup_ptr = uhci_create_data_tds(dev, KVTOP((addr_t)&xfer->xfer_data[0]) /* XXX64 */, xfer->xfer_length, xfer->xfer_device->usb_max_packet_sz0, isread ? TD_PID_IN : TD_PID_OUT, ls, token_addr, td_handshake, NULL);
	} else {
		next_setup_ptr = td_handshake;
	}

	/* Create the SETUP stage packet (SETUP + DATA0) */
	struct UHCI_HCD_TD* td_setup = uhci_alloc_td(dev);
	td_setup->td_td.td_linkptr = TO_REG32(TD_LINKPTR_VF | uhci_td_get_phys(next_setup_ptr));
	td_setup->td_td.td_status = TO_REG32(ls | TD_STATUS_ACTIVE | TD_STATUS_INTONERR(3));
	td_setup->td_td.td_token = TO_REG32(TD_TOKEN_MAXLEN(sizeof(struct USB_CONTROL_REQUEST)) | token_addr | TD_TOKEN_PID(TD_PID_SETUP));
	td_setup->td_td.td_buffer = KVTOP((addr_t)&xfer->xfer_control_req); /* XXX64 TODO */
	LIST_NEXT(td_setup) = next_setup_ptr;

	/* Schedule an item; this will cause the IRQ to handle our request - XXX needs lock */
	struct UHCI_SCHEDULED_ITEM* si = kmalloc(sizeof *si);
	si->si_td = td_setup;
	si->si_xfer = xfer;
	LIST_APPEND(&p->uhci_scheduled_items, si);

	/* Finally, hand the chain to the HD; it's ready to be transmitted */
	/* XXX we should add to the chain not overwrite !!! */
	p->uhci_qh_ls_control->qh_first_td = td_setup;
	p->uhci_qh_ls_control->qh_qh.qh_elementptr = TO_REG32(uhci_td_get_phys(td_setup));

	//uhci_dump(dev);
	return ANANAS_ERROR_NONE;
}

static errorcode_t
uhci_interrupt_schedule_xfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct UHCI_PRIVDATA* p = dev->privdata;
	struct UHCI_DEV_PRIVDATA* hcd_privdata = xfer->xfer_device->usb_hcd_privdata;
	int ls = (hcd_privdata->dev_flags & UHCI_DEV_FLAG_LOWSPEED) ? TD_STATUS_LS : 0;
	uint32_t token_addr = TD_TOKEN_ENDPOINT(xfer->xfer_endpoint) | TD_TOKEN_ADDRESS(xfer->xfer_address);
	int isread = xfer->xfer_flags & TRANSFER_FLAG_READ;

	struct UHCI_HCD_TD* last_td;
	struct UHCI_HCD_TD* td_chain = uhci_create_data_tds(dev, KVTOP((addr_t)&xfer->xfer_data[0]) /* XXX64 */, xfer->xfer_length, xfer->xfer_device->usb_max_packet_sz0, isread ? TD_PID_IN : TD_PID_OUT, ls, token_addr, NULL, &last_td);

	last_td->td_td.td_status |= TD_STATUS_IOC;
	td_chain->td_td.td_token &= ~TD_TOKEN_DATA; /* XXX */

	/* Schedule an item; this will cause the IRQ to handle our request - XXX needs lock */
	struct UHCI_SCHEDULED_ITEM* si = kmalloc(sizeof *si);
	si->si_td = td_chain;
	si->si_xfer = xfer;
	LIST_APPEND(&p->uhci_scheduled_items, si);

	/* Finally, hand the chain to the HD; it's ready to be transmitted */
	/* XXX we should add to the chain not overwrite !!! */
	int index = 0;
	p->uhci_qh_interrupt[index]->qh_first_td = td_chain;
	p->uhci_qh_interrupt[index]->qh_qh.qh_elementptr = TO_REG32(uhci_td_get_phys(td_chain));

	return ANANAS_ERROR_NONE;
}

static errorcode_t
uhci_schedule_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	mutex_assert(&xfer->xfer_device->usb_mutex, MTX_LOCKED);

	/*
	 * Add the transfer to our pending list; this is done so we can cancel any
	 * pending transfers when a device is removed, for example.
	 */
	KASSERT((xfer->xfer_flags & TRANSFER_FLAG_PENDING) == 0, "scheduling transfer that is already pending (%x)", xfer->xfer_flags);
	xfer->xfer_flags |= TRANSFER_FLAG_PENDING;
	LIST_APPEND_IP(&xfer->xfer_device->usb_transfers, pending, xfer);
	errorcode_t err = ANANAS_ERROR_NONE;

	/* If this is the root hub, short-circuit the request */
	struct UHCI_DEV_PRIVDATA* dev_p = xfer->xfer_device->usb_hcd_privdata;
	if (dev_p->dev_flags & USB_DEVICE_FLAG_ROOT_HUB)
		return uroothub_handle_transfer(dev, xfer);

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
	pci_write_cfg(dev, UHCI_PCI_LEGSUPP, UHCI_LEGSUPP_PIRQEN, 16);

  void* res_io = device_alloc_resource(dev, RESTYPE_IO, 16);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	/* Create DMA tags; we need these to do DMA */
	errorcode_t err = dma_tag_create(dev->parent->dma_tag, dev, &dev->dma_tag, 1, 0, DMA_ADDR_MAX_32BIT, DMA_SEGS_MAX_ANY, DMA_SEGS_MAX_SIZE);
	ANANAS_ERROR_RETURN(err);

	struct UHCI_PRIVDATA* p = kmalloc(sizeof *p);
	memset(p, 0, sizeof *p);
	dev->privdata = p;
	p->uhci_io = (uint32_t)(uintptr_t)res_io;
	mutex_init(&p->uhci_mtx, "uhci");
	LIST_INIT(&p->uhci_scheduled_items);

	/* Allocate the frame list; this will be programmed right into the controller */
	err = dma_buf_alloc(dev->dma_tag, 4096, &p->uhci_framelist_buf);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	p->uhci_framelist = dma_buf_get_segment(p->uhci_framelist_buf, 0)->s_virt;
	KASSERT((((addr_t)p->uhci_framelist) & 0x3ff) == 0, "framelist misaligned");

	/* Disable interrupts; we don't want the messing along */
	outw(p->uhci_io + UHCI_REG_USBINTR, 0);

	/*
	 * Allocate our interrupt, control and bulk queue's. These will be used for
	 * every frame like the following:
	 *
	 * (framelist) -> (isochronous td) -> interrupt -> control -> bulk -> (end)
	 */
	for (unsigned int n = 0; n < UHCI_NUM_INTERRUPT_QH; n++)
		p->uhci_qh_interrupt[n] = uhci_alloc_qh(dev);
	p->uhci_qh_ls_control = uhci_alloc_qh(dev);
	p->uhci_qh_fs_control = uhci_alloc_qh(dev);
	p->uhci_qh_bulk = uhci_alloc_qh(dev);

#define UHCI_LINK_QH(qh, next_qh) \
	(qh)->qh_qh.qh_headptr = TO_REG32(QH_PTR_QH | uhci_qh_get_phys((next_qh))); \
	(qh)->qh_next_qh = (next_qh)

	for (unsigned int n = UHCI_NUM_INTERRUPT_QH - 1; n > 0; n--) {
		UHCI_LINK_QH(p->uhci_qh_interrupt[n], p->uhci_qh_interrupt[n - 1]);
	}

	UHCI_LINK_QH(p->uhci_qh_interrupt[0], p->uhci_qh_ls_control);
	UHCI_LINK_QH(p->uhci_qh_ls_control, p->uhci_qh_fs_control);
	UHCI_LINK_QH(p->uhci_qh_fs_control, p->uhci_qh_bulk);

#undef UHCI_HOOK_QH

	/*
	 * Set up the frame list; we add a dummy TD for isochronous data and then the
	 * QH's set up above. Interrupt[0] is every 1ms, [1] is every 2ms etc.
	 */
	for (int i = 0; i < UHCI_FRAMELIST_LEN; i++) {
		int index = 0; /* 1 ms */
		switch(i & 31) {
			case 1: index = 1; break; /* 2ms */
			case 2: index = 2; break; /* 4ms */
			case 4: index = 3; break; /* 8ms */
			case 8: index = 4; break; /* 16ms */
			case 16: index = 5; break; /* 32ms */
		}
		p->uhci_framelist[i] = TO_REG32(TD_LINKPTR_QH | uhci_qh_get_phys(p->uhci_qh_interrupt[index]));
	}

	/*
	 * Grab a copy of the SOF modify register; we have no idea what it should be,
	 * so we just rely on the BIOS doing the right thing.
	 */
	p->uhci_sof_modify = inw(p->uhci_io + UHCI_REG_SOF);

	/* Reset the host controller */
	outw(p->uhci_io + UHCI_REG_USBCMD, UHCI_USBCMD_GRESET);
	delay(10);
	outw(p->uhci_io + UHCI_REG_USBCMD, 0);
	delay(1);

	/* Now issue a host controller reset and wait until it is done */
	outw(p->uhci_io + UHCI_REG_USBCMD, UHCI_USBCMD_HCRESET);
	int timeout = 50000;
	while (timeout > 0) {
		if ((inw(p->uhci_io + UHCI_REG_USBCMD) & UHCI_USBCMD_HCRESET) == 0)
			break;
		timeout--;
	}
	if (timeout == 0)
		device_printf(dev, "warning: no response on reset");

	/*
	 * Program the USB frame number, start of frame and frame list address base
	 * registers.
	 */
	outw(p->uhci_io + UHCI_REG_FRNUM, 0);
	outw(p->uhci_io + UHCI_REG_SOF, p->uhci_sof_modify);
	outl(p->uhci_io + UHCI_REG_FLBASEADD, TO_REG32(dma_buf_get_segment(p->uhci_framelist_buf, 0)->s_phys));

	/* Tell the USB controller to start pumping frames */
	outw(p->uhci_io + UHCI_REG_USBCMD, UHCI_USBCMD_MAXP | UHCI_USBCMD_RS);
	delay(10);
	if (inw(p->uhci_io + UHCI_REG_USBSTS) & UHCI_USBSTS_HCHALTED) {
		device_printf(dev, "controller does not start");
		goto fail;
	}

	/* Hook up our interrupt handler and get it going */
	err = irq_register((int)(uintptr_t)res_irq, dev, uhci_irq, IRQ_TYPE_DEFAULT, NULL);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	outw(p->uhci_io + UHCI_REG_USBINTR, UHCI_USBINTR_SPI | UHCI_USBINTR_IOC | UHCI_USBINTR_RI | UHCI_USBINTR_TOCRC);
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
	p->uhci_rh_numports = 0;
	while (1) {
		int stat = inw(p->uhci_io + UHCI_REG_PORTSC1 + p->uhci_rh_numports * 2);
		if ((stat & UHCI_PORTSC_ALWAYS1) == 0 || (stat == 0xffff))
			break;
		p->uhci_rh_numports++;
	}
	if (p->uhci_rh_numports > 7) {
		device_printf(dev, "detected excessive %d usb ports; aborting");
		return ANANAS_ERROR(NO_DEVICE);
	}

	return ANANAS_ERROR_NONE;

fail:
	kfree(p);
	return err;
}

static void*
uhci_device_init_privdata(int flags)
{
	struct UHCI_DEV_PRIVDATA* privdata = kmalloc(sizeof *privdata);
	memset(privdata, 0, sizeof *privdata);
	privdata->dev_flags = flags;
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
		return ANANAS_ERROR_NONE;
	return ANANAS_ERROR(NO_RESOURCE);
}

static errorcode_t
uhci_probe(device_t dev)
{
	/* XXX This is a crude hack to ensure we only use one UHCI device for now */
	if (dev->unit > 0) return ANANAS_ERROR(NO_RESOURCE);
	return uhci_match_dev(dev);
}

static void
uhci_set_roothub(device_t dev, struct USB_DEVICE* usb_dev)
{
	struct UHCI_PRIVDATA* p = dev->privdata;
	p->uhci_roothub = usb_dev;

	/* Now we can start the roothub thread to service updates */
	uroothub_start(dev);
}

struct DRIVER drv_uhci = {
	.name					= "uhci",
	.drv_probe		= uhci_probe,
	.drv_attach		= uhci_attach,
	.drv_usb_set_roothub = uhci_set_roothub,
	.drv_usb_setup_xfer = uhci_setup_transfer,
	.drv_usb_teardown_xfer = uhci_teardown_transfer,
	.drv_usb_schedule_xfer = uhci_schedule_transfer,
	.drv_usb_cancel_xfer = uhci_cancel_transfer,
	.drv_usb_hcd_initprivdata = uhci_device_init_privdata,
};

DRIVER_PROBE(uhci)
DRIVER_PROBE_BUS(pcibus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
