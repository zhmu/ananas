/*
 * OHCI
 *
 * Periodic transfers
 * ------------------
 * OHCI works with a 32-entry interrupt scheduling pointer; that means a tree
 * should be created as we want to be able to schedule interrupt transfers
 * every 1, 2, 4, 8, 16 and 32ms. OHCI desires 32 periodic lists from us, which
 * should be linked in such a way that the scheduling requirements above apply.
 *
 * An important observation is that there is only a single 1ms list (as every
 * of the periodic list entries must eventually call it) but there are two
 * 2ms lists, 4x 4ms lists, 8x 8ms lists, 16x 16ms lists and 32x 32ms lists -
 * this is because even though each transfer on the list should be performed
 * every N seconds, having multiple transfers that have to occur every N
 * seconds doesn't mean they have to be on the same list.
 *
 * However, this isn't what we currently do (the OHCI sample code does this,
 * and takes care to rebalance the tree to prevent all work (32ms, 16ms, 8ms,
 * 4ms, 2ms and 1ms) from being placed on a single node); we use the
 * naive approach by just creating lists for 1ms .. 32ms transfers and hope
 * they won't overflow for now.
 */
#include <ananas/types.h>
#include <ananas/bus/pci.h>
#include <ananas/bus/usb/core.h>
#include <ananas/error.h>
#include <ananas/dma.h>
#include <ananas/irq.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <ananas/time.h>
#include <ananas/trace.h>
#include "ohci-reg.h"
#include "ohci-roothub.h"
#include "ohci-hcd.h"
#include "usb-device.h"
#include "usb-transfer.h"

#include <machine/vm.h> /* for KVTOP, which must go */

TRACE_SETUP;

struct OHCI_DEV_PRIVDATA {
	int dev_flags;
};

static inline uint32_t
ohci_td_get_phys(struct OHCI_HCD_TD* td)
{
	return dma_buf_get_segment(td->td_buf, 0)->s_phys;
}

static inline uint32_t
ohci_ed_get_phys(struct OHCI_HCD_ED* ed)
{
	return dma_buf_get_segment(ed->ed_buf, 0)->s_phys;
}

static void
ohci_dump_td(struct OHCI_HCD_TD* td)
{
	kprintf("td %x -> flags %p cbp %x nexttd %x be %x\n",
	 ohci_td_get_phys(td),
	 td->td_td.td_flags,
	 td->td_td.td_cbp,
	 td->td_td.td_nexttd,
	 td->td_td.td_be);
}

static void
ohci_dump_ed(struct OHCI_HCD_ED* ed)
{
	kprintf("ed %x -> flags %x tailp %x headp %x nexted %x\n",
	 ohci_ed_get_phys(ed),
	 ed->ed_ed.ed_flags,
	 ed->ed_ed.ed_tailp,
	 ed->ed_ed.ed_headp,
	 ed->ed_ed.ed_nexted);

	for (struct OHCI_HCD_TD* td = ed->ed_firsttd; td != NULL; td = td->qi_next) {
		kprintf(" ");
		ohci_dump_td(td);
	}
}

static void
ohci_dump(device_t dev)
{
	struct OHCI_PRIVDATA* p = dev->privdata;

	device_printf(dev, "hcca %x -> fnum %d dh %x",
	 ohci_read4(dev, OHCI_HCHCCA),
	 p->ohci_hcca->hcca_framenumber,
	 p->ohci_hcca->hcca_donehead
	);
	device_printf(dev, "control %x cmdstat %x intstat %x intena %x intdis %x",
	 ohci_read4(dev, OHCI_HCCONTROL),
	 ohci_read4(dev, OHCI_HCCOMMANDSTATUS),
	 ohci_read4(dev, OHCI_HCINTERRUPTSTATUS),
	 ohci_read4(dev, OHCI_HCINTERRUPTENABLE),
	 ohci_read4(dev, OHCI_HCINTERRUPTDISABLE));
	device_printf(dev, "period_cured %x ctrlhead %x ctrlcur %x bulkhead %x bulkcur %x",
	 ohci_read4(dev, OHCI_HCPERIODCURRENTED),
	 ohci_read4(dev, OHCI_HCCONTROLHEADED),
	 ohci_read4(dev, OHCI_HCCONTROLCURRENTED),
	 ohci_read4(dev, OHCI_HCBULKHEADED),
	 ohci_read4(dev, OHCI_HCBULKCURRENTED));
	device_printf(dev, "rhda %x rhdb %x rhstatus %x rhps[0] %x rhps[1] %x rphs[2] %x",
		ohci_read4(dev, OHCI_HCRHDESCRIPTORA),
		ohci_read4(dev, OHCI_HCRHDESCRIPTORB),
		ohci_read4(dev, OHCI_HCRHSTATUS),
		ohci_read4(dev, OHCI_HCRHPORTSTATUSx),
		ohci_read4(dev, OHCI_HCRHPORTSTATUSx + 4),
		ohci_read4(dev, OHCI_HCRHPORTSTATUSx + 8));
	ohci_dump_ed(p->ohci_control_ed);

	if (!DQUEUE_EMPTY(&p->ohci_scheduled_items)) {
		kprintf("** dumping scheduled items\n");
		DQUEUE_FOREACH_SAFE(&p->ohci_scheduled_items, si, struct OHCI_HCD_SCHEDULED_ITEM) {
			ohci_dump_ed(si->si_ed);
		}
	}

	for (unsigned int n = 0; n < 32; n++) {
		kprintf("hcca_inttab[%d] = %x -> ", n,  p->ohci_hcca->hcca_inttable[n]);
		struct OHCI_HCD_ED* ed = (void*)PTOKV(p->ohci_hcca->hcca_inttable[n]);
		ohci_dump_ed(ed);
	}
}

static irqresult_t
ohci_irq(device_t dev, void* context)
{
	struct OHCI_PRIVDATA* p = dev->privdata;

	/*
	 * Obtain the interrupt status. Note that donehead is funky; bit 0 indicates
	 * whethere standard processing is required (if not, this interrupt only
	 * indicates hcca_donehead was updated)
	 */
	uint32_t is = 0;
	uint32_t dh = p->ohci_hcca->hcca_donehead;
	if (dh != 0) {
		is = OHCI_IS_WDH;
		if (dh & 1)
			is |= ohci_read4(dev, OHCI_HCINTERRUPTSTATUS) & ohci_read4(dev, OHCI_HCINTERRUPTENABLE);
	} else {
		is = ohci_read4(dev, OHCI_HCINTERRUPTSTATUS) & ohci_read4(dev, OHCI_HCINTERRUPTENABLE);
	}
	if (is == 0) {
		/* Not my interrupt; ignore */
		return IRQ_RESULT_PROCESSED; // IRQ_RESULT_IGNORED;
	}

	if (is & OHCI_IS_WDH) {
		/*
		 * Done queue has been updated; need to walk through all our scheduled items.
		 */
		if (!DQUEUE_EMPTY(&p->ohci_scheduled_items)) {
			DQUEUE_FOREACH_SAFE(&p->ohci_scheduled_items, si, struct OHCI_HCD_SCHEDULED_ITEM) {
				struct OHCI_HCD_TD* first_td = si->si_ed->ed_firsttd;
				if (first_td->td_td.td_cbp != 0)
					continue; /* not finished */
				DQUEUE_REMOVE(&p->ohci_scheduled_items, si);

				si->si_xfer->xfer_result_length = si->si_xfer->xfer_length;
				usbtransfer_complete(si->si_xfer);
			}
		}
		p->ohci_hcca->hcca_donehead = 0; /* acknowledge donehead */
		ohci_write4(dev, OHCI_HCINTERRUPTSTATUS, OHCI_IS_WDH);
	}
	if (is & OHCI_IS_SO) {
		device_printf(dev, "scheduling overrun!");
		ohci_dump(dev);
		for(;;);
		ohci_write4(dev, OHCI_HCINTERRUPTSTATUS, OHCI_IS_SO);
	}

	if (is & OHCI_IS_FNO) {
		device_printf(dev, "frame num overflow!");
		ohci_dump(dev);

		ohci_write4(dev, OHCI_HCINTERRUPTSTATUS, OHCI_IS_FNO);
	}

	if (is & OHCI_IS_RHSC) { 
		ohci_roothub_irq(dev);

		/*
		 * Disable the roothub irq, we'll re-enable it when the port has been reset
		 * to avoid excessive interrupts.
		 */
		ohci_write4(dev, OHCI_HCINTERRUPTDISABLE, OHCI_ID_RHSC);
		ohci_write4(dev, OHCI_HCINTERRUPTSTATUS, OHCI_IS_RHSC);
	}

	/* If we got anything unexpected, warn and disable them */
	is &= ~(OHCI_IS_WDH | OHCI_IS_SO | OHCI_IS_FNO | OHCI_IS_RHSC);
	if (is != 0) {
		device_printf(dev, "disabling excessive interrupts %x", is);
		ohci_write4(dev, OHCI_HCINTERRUPTDISABLE, is);
	}

	return IRQ_RESULT_PROCESSED;
}

static struct OHCI_HCD_TD*
ohci_alloc_td(device_t dev)
{
	dma_buf_t buf;
	errorcode_t err = dma_buf_alloc(dev->dma_tag, sizeof(struct OHCI_HCD_TD), &buf);
	if (err != ANANAS_ERROR_NONE)
		return NULL;

	struct OHCI_HCD_TD* td = dma_buf_get_segment(buf, 0)->s_virt;
	memset(td, 0, sizeof(struct OHCI_HCD_TD));
	td->td_buf = buf;
	return td;
}

static struct OHCI_HCD_ED*
ohci_alloc_ed(device_t dev)
{
	dma_buf_t buf;
	errorcode_t err = dma_buf_alloc(dev->dma_tag, sizeof(struct OHCI_HCD_ED), &buf);
	if (err != ANANAS_ERROR_NONE)
		return NULL;

	struct OHCI_HCD_ED* ed = dma_buf_get_segment(buf, 0)->s_virt;
	memset(ed, 0, sizeof(struct OHCI_HCD_ED));
	ed->ed_buf = buf;
	return ed;
}

static void
ohci_set_td_next(struct OHCI_HCD_TD* td, struct OHCI_HCD_TD* next)
{
	td->td_td.td_nexttd = ohci_td_get_phys(next);
	td->qi_next = next;
}

/* Inserts 'ed' at 'list', keeping the chain intact */
static inline void
ohci_insert_ed(struct OHCI_HCD_ED* list, struct OHCI_HCD_ED* ed)
{
	ed->ed_ed.ed_nexted = list->ed_ed.ed_nexted;
	list->ed_ed.ed_nexted = ohci_ed_get_phys(ed);
}

static errorcode_t
ohci_schedule_control_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct OHCI_PRIVDATA* p = dev->privdata;
	struct OHCI_DEV_PRIVDATA* devp = xfer->xfer_device->usb_hcd_privdata;
	int is_read = xfer->xfer_flags & TRANSFER_FLAG_READ;
	int is_ls = devp->dev_flags & USB_DEVICE_FLAG_LOW_SPEED;

	/* Construct the SETUP transfer descriptor for this control transfer */
	struct OHCI_HCD_TD* td_setup = ohci_alloc_td(dev);
	td_setup->td_td.td_flags =
	 OHCI_TD_DP(OHCI_TD_DP_SETUP) |
	 OHCI_TD_DI(OHCI_TD_DI_NONE) |
	 OHCI_TD_T(2);
	td_setup->td_td.td_cbp = KVTOP((addr_t)&xfer->xfer_control_req); /* XXX64 TODO */
	td_setup->td_td.td_be = td_setup->td_td.td_cbp + sizeof(struct USB_CONTROL_REQUEST) - 1;

	/* Construct the DATA transfer descriptor */
	struct OHCI_HCD_TD* td_data = NULL;
	if (xfer->xfer_flags & TRANSFER_FLAG_DATA) {
		td_data = ohci_alloc_td(dev);
		td_data->td_td.td_flags =
		 OHCI_TD_R |
		 OHCI_TD_T(3) |
		 OHCI_TD_DI(OHCI_TD_DI_NONE) |
		 OHCI_TD_DP(is_read ? OHCI_TD_DP_IN : OHCI_TD_DP_OUT);
		td_data->td_td.td_cbp = KVTOP((addr_t)&xfer->xfer_data[0]); /* XXX64 */
		td_data->td_td.td_be = td_data->td_td.td_cbp + xfer->xfer_length - 1;
	}

	/* Construct the HANDSHAKE descriptor */
	struct OHCI_HCD_TD* td_handshake = ohci_alloc_td(dev);
	td_handshake->td_td.td_flags =
	 OHCI_TD_DI(OHCI_TD_DI_IMMEDIATE) |
	 OHCI_TD_T(3) |
	 OHCI_TD_DP(is_read ? OHCI_TD_DP_OUT : OHCI_TD_DP_IN);

	/* Construct a dummy tail descriptor */
	struct OHCI_HCD_TD* td_tail = ohci_alloc_td(dev);

	/* Construct an endpoint descriptor for this device */
	struct OHCI_HCD_ED* ed = ohci_alloc_ed(dev);
	ed->ed_ed.ed_flags =
	 OHCI_ED_FA(xfer->xfer_address) |
	 OHCI_ED_EN(xfer->xfer_endpoint) |
	 OHCI_ED_D(OHCI_ED_D_TD) |
	 OHCI_ED_MPS(xfer->xfer_device->usb_max_packet_sz0) |
	 (is_ls ? OHCI_ED_S : 0);

	/* Hook all transfers together: setup -> (data) -> handshake -> tail */
	ohci_set_td_next(td_handshake, td_tail);
	if (xfer->xfer_flags & TRANSFER_FLAG_DATA) {
		ohci_set_td_next(td_data, td_handshake);
		ohci_set_td_next(td_setup, td_data);
	} else {
		ohci_set_td_next(td_setup, td_handshake);
	}
	ed->ed_ed.ed_headp = ohci_td_get_phys(td_setup);
	ed->ed_ed.ed_tailp = ohci_td_get_phys(td_tail);
	ed->ed_firsttd = td_setup;

	/* Add an entry to the scheduled item queue */
	struct OHCI_HCD_SCHEDULED_ITEM* si = kmalloc(sizeof *si);
	si->si_ed = ed;
	si->si_xfer = xfer;
	DQUEUE_ADD_TAIL(&p->ohci_scheduled_items, si);

	/* ... and off it goes! */
	ohci_insert_ed(p->ohci_control_ed, ed);
	ohci_write4(dev, OHCI_HCCOMMANDSTATUS, OHCI_CS_CLF);
	return ANANAS_ERROR_NONE;
}

static errorcode_t
ohci_schedule_interrupt_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct OHCI_PRIVDATA* p = dev->privdata;
	struct OHCI_DEV_PRIVDATA* devp = xfer->xfer_device->usb_hcd_privdata;
	int is_read = xfer->xfer_flags & TRANSFER_FLAG_READ;
	int is_ls = devp->dev_flags & USB_DEVICE_FLAG_LOW_SPEED;

	/* Construct the DATA transfer descriptor */
	struct OHCI_HCD_TD* td_data = NULL;
	td_data = ohci_alloc_td(dev);
	td_data->td_td.td_flags =
	 OHCI_TD_R |
	 OHCI_TD_T(3) |
	 //OHCI_TD_DI(OHCI_TD_DI_NONE) |
	 OHCI_TD_DI(OHCI_TD_DI_IMMEDIATE) |
	 OHCI_TD_DP(is_read ? OHCI_TD_DP_IN : OHCI_TD_DP_OUT);
	td_data->td_td.td_cbp = KVTOP((addr_t)&xfer->xfer_data[0]); /* XXX64 */
	td_data->td_td.td_be = td_data->td_td.td_cbp + xfer->xfer_length - 1;

	/* Construct a dummy tail descriptor */
	struct OHCI_HCD_TD* td_tail = ohci_alloc_td(dev);

	/* Construct an endpoint descriptor for this device */
	struct OHCI_HCD_ED* ed = ohci_alloc_ed(dev);
	ed->ed_ed.ed_flags =
	 OHCI_ED_FA(xfer->xfer_address) |
	 OHCI_ED_EN(xfer->xfer_endpoint) |
	 OHCI_ED_D(OHCI_ED_D_TD) |
	 OHCI_ED_MPS(xfer->xfer_device->usb_max_packet_sz0) | /* XXX Wrong should be pipe's */
	 (is_ls ? OHCI_ED_S : 0);

	/* Hook all transfers together: setup -> (data) -> handshake -> tail */
	ohci_set_td_next(td_data, td_tail);
	ed->ed_ed.ed_headp = ohci_td_get_phys(td_data);
	ed->ed_ed.ed_tailp = ohci_td_get_phys(td_tail);
	ed->ed_firsttd = td_data;

	/* Add an entry to the scheduled item queue */
	struct OHCI_HCD_SCHEDULED_ITEM* si = kmalloc(sizeof *si);
	si->si_ed = ed;
	si->si_xfer = xfer;
	DQUEUE_ADD_TAIL(&p->ohci_scheduled_items, si);

	/* ... and off it goes - XXX LOCK */
	int ed_list = 0; /* XXX */
	struct OHCI_HCD_ED* queue_ed = p->ohci_interrupt_ed[ed_list];
	ohci_insert_ed(queue_ed, ed);

	return ANANAS_ERROR_NONE;
}

static errorcode_t
ohci_schedule_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct OHCI_PRIVDATA* p = dev->privdata;

	/*
	 * Add the transfer to our pending list; this is done so we can cancel any
	 * pending transfers when a device is removed, for example.
	 */
	xfer->xfer_flags |= TRANSFER_FLAG_PENDING;
	mutex_lock(&p->ohci_mtx);
	DQUEUE_ADD_TAIL_IP(&xfer->xfer_device->usb_transfers, pending, xfer);
	mutex_unlock(&p->ohci_mtx);

	/* If this is the root hub, immediately transfer the request to it */
	struct OHCI_DEV_PRIVDATA* dev_p = xfer->xfer_device->usb_hcd_privdata;
	if (dev_p->dev_flags & USB_DEVICE_FLAG_ROOT_HUB) {
		return oroothub_handle_transfer(dev, xfer);
	}

	errorcode_t err;
	switch(xfer->xfer_type) {
		case TRANSFER_TYPE_CONTROL:
			err = ohci_schedule_control_transfer(dev, xfer);
			break;
		case TRANSFER_TYPE_INTERRUPT:
			err = ohci_schedule_interrupt_transfer(dev, xfer);
			break;
		default:
			panic("implement type %d", xfer->xfer_type);
	}

	if (err != ANANAS_ERROR_NONE) {
		xfer->xfer_flags &= ~TRANSFER_FLAG_PENDING;
		mutex_lock(&p->ohci_mtx);
		DQUEUE_REMOVE_IP(&xfer->xfer_device->usb_transfers, pending, xfer);
		mutex_unlock(&p->ohci_mtx);
	}

	return err;
}

static errorcode_t
ohci_cancel_transfer(device_t dev, struct USB_TRANSFER* xfer)
{
	struct OHCI_PRIVDATA* p = dev->privdata;

	if (xfer->xfer_flags & TRANSFER_FLAG_PENDING) {
		xfer->xfer_flags &= ~TRANSFER_FLAG_PENDING;
		mutex_lock(&p->ohci_mtx);
		DQUEUE_REMOVE_IP(&xfer->xfer_device->usb_transfers, pending, xfer);
		mutex_unlock(&p->ohci_mtx);
	}

	/* XXX we should see if we're still running it */
	return ANANAS_ERROR_NONE;
}

static void*
ohci_device_init_privdata(int flags)
{
	struct OHCI_DEV_PRIVDATA* p = kmalloc(sizeof *p);
	memset(p, 0, sizeof *p);
	p->dev_flags = flags;
	return p;
}

static errorcode_t
ohci_setup(device_t dev)
{
	struct OHCI_PRIVDATA* p = dev->privdata;

	/* Allocate and initialize the HCCA structure */
	errorcode_t err = dma_buf_alloc(dev->dma_tag, sizeof(struct OHCI_HCCA), &p->ohci_hcca_buf);
	ANANAS_ERROR_RETURN(err);
	p->ohci_hcca = dma_buf_get_segment(p->ohci_hcca_buf, 0)->s_virt;
	memset(p->ohci_hcca, 0, sizeof(struct OHCI_HCCA));

	/*
	 * Create our lists; every new list should contain a back pointer to the
	 * previous one - we use powers of two for each interval, so every Xms is
	 * also in every 2*X ms.
	 */
	for (unsigned int n = 0; n < OHCI_NUM_ED_LISTS; n++) {
		struct OHCI_HCD_ED* ed = ohci_alloc_ed(dev);
		KASSERT(ed != NULL, "out of eds");
		p->ohci_interrupt_ed[n] = ed;
		ed->ed_ed.ed_flags = OHCI_ED_K;

		if (n > 0)
			ed->ed_ed.ed_nexted = dma_buf_get_segment(p->ohci_interrupt_ed[n - 1]->ed_buf, 0)->s_phys;
		else
			ed->ed_ed.ed_nexted = 0;
	}

	/* Hook up the lists to the interrupt transfer table XXX find formula for this */
	const unsigned int tables[32] = {
		0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
		0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5
	};
	(void)tables;
	for (unsigned int n = 0; n < 32; n++) {
		p->ohci_hcca->hcca_inttable[n] = dma_buf_get_segment(p->ohci_interrupt_ed[tables[n]]->ed_buf, 0)->s_phys;
	}

	/* Allocate a control/bulk transfer ED */
	p->ohci_control_ed = ohci_alloc_ed(dev);
	p->ohci_bulk_ed = ohci_alloc_ed(dev);
	KASSERT(p->ohci_control_ed != NULL && p->ohci_bulk_ed != NULL, "out of ed");

	p->ohci_control_ed->ed_ed.ed_flags = OHCI_ED_K;
	p->ohci_bulk_ed->ed_ed.ed_flags = OHCI_ED_K;
	return ANANAS_ERROR_NONE;
}

static errorcode_t
ohci_attach(device_t dev)
{
  void* res_mem = device_alloc_resource(dev, RESTYPE_MEMORY, 4096);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_mem == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);
	pci_enable_busmaster(dev, 1);

	/* See if the revision makes sense; if not, we can't attach to this */
	uint32_t rev = *(volatile uint32_t*)((char*)res_mem + OHCI_HCREVISION);
	if (OHCI_REVISION(rev) != 0x10) {
		device_printf(dev, "unsupported revision 0x%x", OHCI_REVISION(rev));
		return ANANAS_ERROR(NO_DEVICE);
	}

	/* Allocate DMA tags */
  errorcode_t err = dma_tag_create(dev->parent->dma_tag, dev, &dev->dma_tag, 1, 0, DMA_ADDR_MAX_32BIT, 1, DMA_SEGS_MAX_SIZE);
	ANANAS_ERROR_RETURN(err);

	/*
	 * Okay, we should be able to support this; initialize our private data so we can use
	 * ohci_{read,write}[24].
	 */
	struct OHCI_PRIVDATA* p = kmalloc(sizeof *p);
	memset(p, 0, sizeof *p);
	p->ohci_membase = res_mem;
	mutex_init(&p->ohci_mtx, "ohci");
	dev->privdata = p;

	/* Set up the interrupt handler */
	err = irq_register((int)(uintptr_t)res_irq, dev, ohci_irq, p);
	ANANAS_ERROR_RETURN(err);

	/* Initialize the structures */
	err = ohci_setup(dev);
	ANANAS_ERROR_RETURN(err);

	if (ohci_read4(dev, OHCI_HCCONTROL) & OHCI_CONTROL_IR) {
		/* Controller is in SMM mode - we need to ask it to stop doing that */
		ohci_write4(dev, OHCI_HCCOMMANDSTATUS, OHCI_CS_OCR);
		int n = 5000;
		while (n > 0 && ohci_read4(dev, OHCI_HCCONTROL) & OHCI_CONTROL_IR)
			n--; /* XXX kludge; should use a real timeout mechanism */
		if (n == 0) {
			device_printf(dev, "stuck in smm, giving up");
			return ANANAS_ERROR(NO_DEVICE);
		}
	}

	/* Initialize our root hub; actual attaching/probing is handled by usbbus */
	err = oroothub_init(dev);
	ANANAS_ERROR_RETURN(err);

	/* Kludge: force reset state */
	ohci_write4(dev, OHCI_HCCONTROL, OHCI_CONTROL_HCFS(OHCI_HCFS_USBRESET));
	
	/* Save contents of 'frame interval' and reset the HC */
	uint32_t fi = OHCI_FM_FI(ohci_read4(dev, OHCI_HCFMINTERVAL));
	ohci_write4(dev, OHCI_HCCOMMANDSTATUS, OHCI_CS_HCR);
	int n = 5000;
	while (n > 0) {
		delay(1);
		if ((ohci_read4(dev, OHCI_HCCOMMANDSTATUS) & OHCI_CS_HCR) == 0)
			break;
		n--; /* XXX kludge; should use a real timeout mechanism */
	}
	if (n == 0) {
		device_printf(dev, "stuck in reset, giving up");
		return ANANAS_ERROR(NO_DEVICE);
	}
	/* Now in USBSUSPEND state -> we have 2ms to continue */

	/* Write addresses of our buffers */
	ohci_write4(dev, OHCI_HCHCCA, dma_buf_get_segment(p->ohci_hcca_buf, 0)->s_phys);
	ohci_write4(dev, OHCI_HCCONTROLHEADED, ohci_ed_get_phys(p->ohci_control_ed));
	ohci_write4(dev, OHCI_HCBULKHEADED, ohci_ed_get_phys(p->ohci_bulk_ed));

	/* Enable interrupts */
	ohci_write4(dev, OHCI_HCINTERRUPTDISABLE, 
	 (OHCI_ID_SO | OHCI_ID_WDH | OHCI_ID_SF | OHCI_ID_RD | OHCI_ID_UE | OHCI_ID_FNO | OHCI_ID_RHSC | OHCI_ID_OC)
	);
	int ints_enabled = (
		OHCI_IE_SO | OHCI_IE_WDH | OHCI_IE_RD | OHCI_IE_UE | OHCI_IE_FNO | OHCI_IE_RHSC | OHCI_IE_OC
	);
	ohci_write4(dev, OHCI_HCINTERRUPTENABLE, ints_enabled | OHCI_IE_MIE);

	/* Start sending USB frames */
	uint32_t c = ohci_read4(dev, OHCI_HCCONTROL);
	c &= ~(OHCI_CONTROL_CBSR(OHCI_CBSR_MASK) | OHCI_CONTROL_HCFS(OHCI_HCFS_MASK) | OHCI_CONTROL_PLE | OHCI_CONTROL_IE | OHCI_CONTROL_CLE | OHCI_CONTROL_BLE | OHCI_CONTROL_IR);
	c |= OHCI_CONTROL_PLE | OHCI_CONTROL_IE | OHCI_CONTROL_CLE | OHCI_CONTROL_BLE;
	c |= OHCI_CONTROL_CBSR(OHCI_CBSR_4_1) | OHCI_CONTROL_HCFS(OHCI_HCFS_USBOPERATIONAL);
	ohci_write4(dev, OHCI_HCCONTROL, c);

	/* State is now OPERATIONAL -> we can write interval/periodic start now */
	uint32_t fm = (ohci_read4(dev, OHCI_HCFMINTERVAL) ^ OHCI_FM_FIT); /* invert FIT bit */
	fm |= OHCI_FM_FSMPS(((fi - OHCI_FM_MAX_OVERHEAD) * 6) / 7); /* calculate FSLargestDataPacket */
	fm |= fi; /* restore frame interval */
	ohci_write4(dev, OHCI_HCFMINTERVAL, fm);
	uint32_t ps = (OHCI_FM_FI(fi) * 9) / 10;
	ohci_write4(dev, OHCI_HCPERIODICSTART, ps);

	/* 'Fiddle' with roothub registers to awaken it */
	uint32_t a = ohci_read4(dev, OHCI_HCRHDESCRIPTORA);
	ohci_write4(dev, OHCI_HCRHDESCRIPTORA, a | OHCI_RHDA_NOCP);
	ohci_write4(dev, OHCI_HCRHSTATUS, OHCI_RHS_LPSC); /* set global power */
	delay(10);
	ohci_write4(dev, OHCI_HCRHDESCRIPTORA, a);

	return ANANAS_ERROR_NONE;
}

static int
ohci_match_dev(device_t dev)
{
	struct RESOURCE* class_res  = device_get_resource(dev, RESTYPE_PCI_CLASSREV, 0);
	if (class_res == NULL) /* XXX it's a bug if this happens */
		return ANANAS_ERROR(NO_RESOURCE);
	uint32_t classrev = class_res->base;

	/* Generic OHCI USB device */
	if (PCI_CLASS(classrev) == PCI_CLASS_SERIAL && PCI_SUBCLASS(classrev) == PCI_SUBCLASS_USB && PCI_PROGINT(classrev) == 0x10)
		return ANANAS_ERROR_OK;
	return ANANAS_ERROR(NO_RESOURCE);
}

static errorcode_t
ohci_probe(device_t dev)
{
	/* XXX This is a crude hack to ensure we only use one OHCI device for now */
	if (dev->unit > 0) return ANANAS_ERROR(NO_RESOURCE);
	return ohci_match_dev(dev);
}

static void
ohci_set_roothub(device_t dev, struct USB_DEVICE* usb_dev)
{
	struct OHCI_PRIVDATA* p = dev->privdata;
	p->ohci_roothub = usb_dev;

	/* Now we can start the roothub thread to service updates */
	oroothub_start(dev);
}

struct DRIVER drv_ohci = {
	.name					= "ohci",
	.drv_probe		= ohci_probe,
	.drv_attach		= ohci_attach,
	.drv_usb_schedule_xfer = ohci_schedule_transfer,
	.drv_usb_cancel_xfer = ohci_cancel_transfer,
	.drv_usb_hcd_initprivdata = ohci_device_init_privdata,
	.drv_usb_set_roothub = ohci_set_roothub,
};

DRIVER_PROBE(ohci)
DRIVER_PROBE_BUS(pcibus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
