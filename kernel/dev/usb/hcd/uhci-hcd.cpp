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
#include "kernel/dev/pci.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/dma.h"
#include "kernel/irq.h"
#include "kernel/lib.h"
#include "kernel/list.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/time.h"
#include "kernel/trace.h"
#include "kernel/x86/io.h"
#include "kernel-md/vm.h" // XXX for KVTOP, which should be removed
#include "../core/usb-core.h"
#include "../core/usb-device.h"
#include "../core/usb-transfer.h"
#include "../core/descriptor.h"
#include "uhci-hcd.h"
#include "uhci-roothub.h"
#include "uhci-reg.h"

TRACE_SETUP;

namespace usb {
namespace uhci {

// HCD_{TD,QH} must start with an UHCI{TD,QH} struct - this is what the HCD
// hardware uses! This means we cannot inherit from NodePtr
struct HCD_TD {
	struct UHCI_TD td_td;
	dma_buf_t td_buf;

	struct HCD_TD* td_next;
};

struct HCD_QH {
	struct UHCI_QH qh_qh;
	struct HCD_TD* qh_first_td;
	struct HCD_QH* qh_next_qh;
	dma_buf_t qh_buf;

	util::List<HCD_QH>::Node np_NodePtr;
};

struct HCD_ScheduledItem : util::List<HCD_ScheduledItem>::NodePtr {
	struct HCD_TD* si_td;
	Transfer* si_xfer;
};

namespace {

addr_t
GetPhysicalAddress(HCD_TD* td)
{
	if (td == nullptr)
		return TD_LINKPTR_T;
	return dma_buf_get_segment(td->td_buf, 0)->s_phys;
}

addr_t
GetPhysicalAddress(HCD_QH* qh)
{
	return dma_buf_get_segment(qh->qh_buf, 0)->s_phys;
}

void
DumpTD(HCD_TD* tdd)
{
	if (tdd == nullptr)
		return;

	struct UHCI_TD* td = &tdd->td_td;
	uint32_t td_status = td->td_status;
	kprintf("td [hcd %p td %p] => linkptr [hcd %p td %x] status 0x%x [%c%c%c%c%c%c%c%c%c%c%c]",
	 td, &tdd->td_td,
	 tdd->td_next, td->td_linkptr,
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
	DumpTD(tdd->td_next);
}

void
DumpQH(HCD_QH* qhh)
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
		DumpTD(qhh->qh_first_td);
	}
	if (qh->qh_headptr & QH_PTR_T)
		return;
	if (qh->qh_headptr & QH_PTR_QH)
		DumpQH(qhh->qh_next_qh);
	else
		DumpTD(qhh->qh_first_td); // XXX This is wrong
}

/* Calculates total number of bytes transferred and checks errors */
bool
VerifyChainAndCalculateLength(HCD_TD* td, int* length)
{
	int errors = 0;
	*length = 0;
	for (/* nothing */; td != NULL; td = td->td_next) {
		int len = TD_STATUS_ACTUALLEN(td->td_td.td_status);
		if (len != TD_ACTUALLEN_NONE)
			*length += len;
		if (td->td_td.td_status & (TD_STATUS_STALLED | TD_STATUS_DATABUFERR | TD_STATUS_BABBLE | TD_STATUS_NAK | TD_STATUS_CRCTOERR | TD_STATUS_BITSTUFF))
			errors++;
	}
	return errors == 0;
}

/* Creates a TD-chain for [size] bytes of [data] and links it to [link_td] */
HCD_TD*
CreateDataTDs(UHCI_HCD& hcd, addr_t data, size_t size, int max_packet_size, int token, int ls, int token_addr, struct HCD_TD* link_td, struct HCD_TD** last_td)
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

		struct HCD_TD* td_data = hcd.AllocateTD();
		if (i == 0 && last_td != NULL)
			*last_td = td_data;

		td_data->td_td.td_linkptr = TO_REG32(TD_LINKPTR_VF | GetPhysicalAddress(link_td));
		td_data->td_td.td_status = TO_REG32(ls | TD_STATUS_ACTIVE | TD_STATUS_INTONERR(3));
		td_data->td_td.td_token = TO_REG32(token_addr | TD_TOKEN_MAXLEN(xfer_chunk_len) | TD_TOKEN_PID(token) | (data_token ? TD_TOKEN_DATA : 0));
		td_data->td_td.td_buffer = cur_data_ptr - xfer_chunk_len;
		td_data->td_next = link_td;

		data_token ^= 1;
		link_td = td_data;
		cur_data_ptr -= xfer_chunk_len;
	}
	KASSERT(cur_data_ptr == data, "bug");
	return link_td;
}

} // unnamed namespace

} // namespace uhci

uhci::HCD_TD*
UHCI_HCD::AllocateTD()
{
	dma_buf_t buf;
	if (auto result = dma_buf_alloc(d_DMA_tag, sizeof(struct uhci::HCD_TD), &buf); result.IsFailure())
		return nullptr;

	auto td = static_cast<struct uhci::HCD_TD*>(dma_buf_get_segment(buf, 0)->s_virt);
	memset(td, 0, sizeof(struct uhci::HCD_TD));
	td->td_buf = buf;
	return td;
}

uhci::HCD_QH*
UHCI_HCD::AllocateQH()
{
	dma_buf_t buf;
	if (auto result = dma_buf_alloc(d_DMA_tag, sizeof(struct uhci::HCD_QH), &buf); result.IsFailure())
		return nullptr;

	auto qh = static_cast<struct uhci::HCD_QH*>(dma_buf_get_segment(buf, 0)->s_virt);
	qh->qh_buf = buf;
	qh->qh_first_td = NULL;
	qh->qh_next_qh = NULL;

	/* Initialize the QH; we'll clear all links */
	memset(&qh->qh_qh, 0, sizeof(struct UHCI_QH));
	qh->qh_qh.qh_headptr = TO_REG32(QH_PTR_T);
	qh->qh_qh.qh_elementptr = TO_REG32(QH_PTR_T);
	return qh;
}

#if 0
static void	
uhci_free_td(device_t dev, struct HCD_TD* td)
{
	dma_buf_free(td->td_buf);
}
#endif

void
UHCI_HCD::FreeQH(uhci::HCD_QH* qh)
{
	dma_buf_free(qh->qh_buf);
}

void
UHCI_HCD::Dump()
{
	int frnum     = uhci_Resources.Read2(UHCI_REG_FRNUM) & 0x3ff;
	addr_t flbase = uhci_Resources.Read4(UHCI_REG_FLBASEADD) & 0xfffff000;
	kprintf("uhci dump\n");
	kprintf(" cmd 0x%x", uhci_Resources.Read2(UHCI_REG_USBCMD));
	kprintf(" status 0x%x", uhci_Resources.Read2(UHCI_REG_USBSTS));
	kprintf(" intr 0x%x", uhci_Resources.Read2(UHCI_REG_USBINTR));
	kprintf(" frnum %u",   frnum);
	kprintf(" flbase %p",   flbase);
	kprintf(" sof %u",   uhci_Resources.Read2(UHCI_REG_SOF));
	kprintf(" portsc1 0x%x", uhci_Resources.Read2(UHCI_REG_PORTSC1));
	kprintf(" portsc2 0x%x", uhci_Resources.Read2(UHCI_REG_PORTSC2));
	KASSERT((addr_t)uhci_framelist == PTOKV(flbase), "framelist %p not in framelist base register %p?", uhci_framelist, PTOKV(flbase));
	uint32_t fl_ptr = *(uint32_t*)(PTOKV(flbase) + frnum * sizeof(uint32_t));
	kprintf(" flptr 0x%x\n", fl_ptr);
	KASSERT(((addr_t)fl_ptr & QH_PTR_QH) != 0, "fl_ptr %p: not a qh at the root?", fl_ptr);
	// XXX we should look up the HCD_QH to use
	for (int n = 0; n < UHCI_NUM_INTERRUPT_QH; n++) {
		kprintf("> %d ms\n", 1 << n);
		uhci::DumpQH(uhci_qh_interrupt[n]);
	}
}

irq::IRQResult
UHCI_HCD::OnIRQ()
{
	int stat = uhci_Resources.Read2(UHCI_REG_USBSTS);
	uhci_Resources.Write2(UHCI_REG_USBSTS, stat);

	kprintf("uhci_irq: stat=%x\n", stat);

	if (stat & UHCI_USBSTS_HCHALTED) {
		Printf("ERROR: Host Controller Halted!");
		Dump();
	}

	if (stat & UHCI_USBSTS_HCPE) {
		Printf("ERROR: Host Process Error");
	}

	if (stat & UHCI_USBSTS_HSE) {
		Printf("ERROR: Host System Error");
	}

	if (stat & UHCI_USBSTS_USBINT) {
		/*
		 * We got an interrupt because something was completed; but we have no idea
		 * what it was. We'll have to traverse the scheduled items and wake anything
		 * up that finished.
		 */
		for(auto it = uhci_scheduled_items.begin(); it != uhci_scheduled_items.end(); /* nothing */) {
			auto& si = *it; ++it;

			/*
			 * Transfers are scheduled in such a way that we can use the first TD to
			 * determine whether the transfer went OK (as only the final TD will have the
			 * interrupt-on-completion flag enabled)
			 */
			if (si.si_td->td_td.td_status & TD_STATUS_ACTIVE)
				continue;

			/* First of all, remove the scheduled item - this orphanages the TD's */
			uhci_scheduled_items.remove(si);

			/* Walk through the chain to calculate the length and see if something gave an error */
			if (!uhci::VerifyChainAndCalculateLength(si.si_td, &si.si_xfer->t_result_length))
				si.si_xfer->t_flags |= TRANSFER_FLAG_ERROR;

			/* Finally, give hand the transfer back to the USB stack */
			si.si_xfer->Complete();
			// XXX Where will will we free si ?
		}
	}
	return irq::IRQResult::Processed;
}

Result
UHCI_HCD::SetupTransfer(Transfer& xfer)
{
	/*
	 * Create a Queue Head for the transfer; we'll hook all TD's to this QH, but we'll
	 * only create the TD's in uhci_schedule_transfer() as we won't know the length
	 * beforehand.
	 */
	uhci::HCD_QH* qh = AllocateQH();
	xfer.t_hcd = qh;
	return Result::Success();
}

Result
UHCI_HCD::TearDownTransfer(Transfer& xfer)
{
	if (xfer.t_hcd != nullptr) {
		/* XXX We should ensure it's no longer in use */
		FreeQH(static_cast<struct uhci::HCD_QH*>(xfer.t_hcd));
	}

	xfer.t_hcd = nullptr;
	return Result::Success();
}

Result
UHCI_HCD::CancelTransfer(Transfer& xfer)
{
	auto& usb_dev = xfer.t_device;
	usb_dev.AssertLocked();

	if (xfer.t_flags & TRANSFER_FLAG_PENDING) {
		xfer.t_flags &= ~TRANSFER_FLAG_PENDING;
		usb_dev.ud_transfers.remove(xfer);
	}

	return Result::Success();
}

// XXX Maybe combine with ScheduleInterruptTransfer ?
Result
UHCI_HCD::ScheduleControlTransfer(Transfer& xfer)
{
	int ls = (xfer.t_device.ud_flags & USB_DEVICE_FLAG_LOW_SPEED) ? TD_STATUS_LS : 0;
	uint32_t token_addr = TD_TOKEN_ENDPOINT(xfer.t_endpoint) | TD_TOKEN_ADDRESS(xfer.t_address);
	int isread = xfer.t_flags & TRANSFER_FLAG_READ;

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
	auto td_handshake = AllocateTD();
	td_handshake->td_td.td_linkptr = TO_REG32(TD_LINKPTR_T);
	td_handshake->td_td.td_status = TO_REG32(ls | TD_STATUS_IOC | TD_STATUS_ACTIVE | TD_STATUS_INTONERR(3));
	td_handshake->td_td.td_token = TO_REG32(token_addr | TD_TOKEN_PID(isread ? TD_PID_OUT : TD_PID_IN) | TD_TOKEN_DATA);
	td_handshake->td_td.td_buffer = 0;

	/*
	 * Create the DATA stage packets, if any - note that we make them in *reverse*
	 * order so that we can hook them together immediately (order is very
	 * important as they will be filled from first to last)
	 */
	uhci::HCD_TD* next_setup_ptr;
	if (xfer.t_flags & TRANSFER_FLAG_DATA) {
		next_setup_ptr = uhci::CreateDataTDs(*this, KVTOP((addr_t)&xfer.t_data[0]) /* XXX64 */, xfer.t_length, xfer.t_device.ud_max_packet_sz0, isread ? TD_PID_IN : TD_PID_OUT, ls, token_addr, td_handshake, nullptr);
	} else {
		next_setup_ptr = td_handshake;
	}

	/* Create the SETUP stage packet (SETUP + DATA0) */
	auto td_setup = AllocateTD();
	td_setup->td_td.td_linkptr = TO_REG32(TD_LINKPTR_VF | uhci::GetPhysicalAddress(next_setup_ptr));
	td_setup->td_td.td_status = TO_REG32(ls | TD_STATUS_ACTIVE | TD_STATUS_INTONERR(3));
	td_setup->td_td.td_token = TO_REG32(TD_TOKEN_MAXLEN(sizeof(struct USB_CONTROL_REQUEST)) | token_addr | TD_TOKEN_PID(TD_PID_SETUP));
	td_setup->td_td.td_buffer = KVTOP((addr_t)&xfer.t_control_req); /* XXX64 TODO */
	td_setup->td_next = next_setup_ptr;

	/* Schedule an item; this will cause the IRQ to handle our request - XXX needs lock */
	auto si = new uhci::HCD_ScheduledItem;
	si->si_td = td_setup;
	si->si_xfer = &xfer;
	uhci_scheduled_items.push_back(*si);

	/* Finally, hand the chain to the HD; it's ready to be transmitted */
	/* XXX we should add to the chain not overwrite !!! */
	/* XXX Is this even safe? */
	uhci_qh_ls_control->qh_first_td = td_setup;
	uhci_qh_ls_control->qh_qh.qh_elementptr = TO_REG32(uhci::GetPhysicalAddress(td_setup));

	//uhci_dump(dev);
	return Result::Success();
}

// XXX Maybe combine with ScheduleControlTransfer ?
Result
UHCI_HCD::ScheduleInterruptTransfer(Transfer& xfer)
{
	int ls = (xfer.t_device.ud_flags & USB_DEVICE_FLAG_LOW_SPEED) ? TD_STATUS_LS : 0;
	uint32_t token_addr = TD_TOKEN_ENDPOINT(xfer.t_endpoint) | TD_TOKEN_ADDRESS(xfer.t_address);
	int isread = xfer.t_flags & TRANSFER_FLAG_READ;

	uhci::HCD_TD* last_td;
	auto td_chain = uhci::CreateDataTDs(*this, KVTOP((addr_t)&xfer.t_data[0]) /* XXX64 */, xfer.t_length, xfer.t_device.ud_max_packet_sz0, isread ? TD_PID_IN : TD_PID_OUT, ls, token_addr, nullptr, &last_td);

	last_td->td_td.td_status |= TD_STATUS_IOC;
	td_chain->td_td.td_token &= ~TD_TOKEN_DATA; /* XXX */

	/* Schedule an item; this will cause the IRQ to handle our request - XXX needs lock */
	auto si = new uhci::HCD_ScheduledItem;
	si->si_td = td_chain;
	si->si_xfer = &xfer;
	uhci_scheduled_items.push_back(*si);

	/* Finally, hand the chain to the HD; it's ready to be transmitted */
	/* XXX we should add to the chain not overwrite !!! */
	int index = 0;
	uhci_qh_interrupt[index]->qh_first_td = td_chain;
	uhci_qh_interrupt[index]->qh_qh.qh_elementptr = TO_REG32(uhci::GetPhysicalAddress(td_chain));

	return Result::Success();
}

Result
UHCI_HCD::ScheduleTransfer(Transfer& xfer)
{
	auto& usb_dev = xfer.t_device;
	usb_dev.AssertLocked();

	/*
	 * Add the transfer to our pending list; this is done so we can cancel any
	 * pending transfers when a device is removed, for example.
	 */
	KASSERT((xfer.t_flags & TRANSFER_FLAG_PENDING) == 0, "scheduling transfer that is already pending (%x)", xfer.t_flags);
	xfer.t_flags |= TRANSFER_FLAG_PENDING;
	usb_dev.ud_transfers.push_back(xfer);

	/* If this is the root hub, short-circuit the request */
	if (usb_dev.ud_flags & USB_DEVICE_FLAG_ROOT_HUB)
		return uhci_RootHub->HandleTransfer(xfer);

	switch(xfer.t_type) {
		case TRANSFER_TYPE_CONTROL:
			return ScheduleControlTransfer(xfer);
		case TRANSFER_TYPE_INTERRUPT:
			return ScheduleInterruptTransfer(xfer);
	}

	panic("unsupported transfer type %u", xfer.t_type);
}

Result
UHCI_HCD::Attach()
{
	/*
	 * Disable legacy PS/2 support; we do not want it, and it can only interfere
	 * with our operations.
	 */
	pci_write_cfg(*this, UHCI_PCI_LEGSUPP, UHCI_LEGSUPP_PIRQEN, 16);

	void* res_io = d_ResourceSet.AllocateResource(Resource::RT_IO, 16);
	void* res_irq = d_ResourceSet.AllocateResource(Resource::RT_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return RESULT_MAKE_FAILURE(ENODEV);

	/* Create DMA tags; we need these to do DMA */
	if (auto result = dma_tag_create(d_Parent->d_DMA_tag, *this, &d_DMA_tag, 1, 0, DMA_ADDR_MAX_32BIT, DMA_SEGS_MAX_ANY, DMA_SEGS_MAX_SIZE); result.IsFailure())
		return result;

	uhci_Resources = uhci::HCD_Resources((uint32_t)(uintptr_t)res_io);

	/* Allocate the frame list; this will be programmed right into the controller */
	if (auto result = dma_buf_alloc(d_DMA_tag, 4096, &uhci_framelist_buf); result.IsFailure())
		return result;
	uhci_framelist = static_cast<uint32_t*>(dma_buf_get_segment(uhci_framelist_buf, 0)->s_virt);
	KASSERT((((addr_t)uhci_framelist) & 0x3ff) == 0, "framelist misaligned");

	/* Disable interrupts; we don't want the messing along */
	uhci_Resources.Write2(UHCI_REG_USBINTR, 0);

	/*
	 * Allocate our interrupt, control and bulk queue's. These will be used for
	 * every frame like the following:
	 *
	 * (framelist) -> (isochronous td) -> interrupt -> control -> bulk -> (end)
	 */
	for (unsigned int n = 0; n < UHCI_NUM_INTERRUPT_QH; n++)
		uhci_qh_interrupt[n] = AllocateQH();
	uhci_qh_ls_control = AllocateQH();
	uhci_qh_fs_control = AllocateQH();
	uhci_qh_bulk = AllocateQH();

#define UHCI_LINK_QH(qh, next_qh) \
	(qh)->qh_qh.qh_headptr = TO_REG32(QH_PTR_QH | uhci::GetPhysicalAddress((next_qh))); \
	(qh)->qh_next_qh = (next_qh)

	for (unsigned int n = UHCI_NUM_INTERRUPT_QH - 1; n > 0; n--) {
		UHCI_LINK_QH(uhci_qh_interrupt[n], uhci_qh_interrupt[n - 1]);
	}

	UHCI_LINK_QH(uhci_qh_interrupt[0], uhci_qh_ls_control);
	UHCI_LINK_QH(uhci_qh_ls_control, uhci_qh_fs_control);
	UHCI_LINK_QH(uhci_qh_fs_control, uhci_qh_bulk);

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
		uhci_framelist[i] = TO_REG32(TD_LINKPTR_QH | uhci::GetPhysicalAddress(uhci_qh_interrupt[index]));
	}

	/*
	 * Grab a copy of the SOF modify register; we have no idea what it should be,
	 * so we just rely on the BIOS doing the right thing.
	 */
	uhci_sof_modify = uhci_Resources.Read2(UHCI_REG_SOF);

	/* Reset the host controller */
	uhci_Resources.Write2(UHCI_REG_USBCMD, UHCI_USBCMD_GRESET);
	delay(10);
	uhci_Resources.Write2(UHCI_REG_USBCMD, 0);
	delay(1);

	/* Now issue a host controller reset and wait until it is done */
	uhci_Resources.Write2(UHCI_REG_USBCMD, UHCI_USBCMD_HCRESET);
	{
		int timeout = 50000;
		while (timeout > 0) {
			if ((uhci_Resources.Read2(UHCI_REG_USBCMD) & UHCI_USBCMD_HCRESET) == 0)
				break;
			timeout--;
		}
		if (timeout == 0)
			Printf("warning: no response on reset");
	}

	/*
	 * Program the USB frame number, start of frame and frame list address base
	 * registers.
	 */
	uhci_Resources.Write2(UHCI_REG_FRNUM, 0);
	uhci_Resources.Write2(UHCI_REG_SOF, uhci_sof_modify);
	uhci_Resources.Write4(UHCI_REG_FLBASEADD, TO_REG32(dma_buf_get_segment(uhci_framelist_buf, 0)->s_phys));

	/* Tell the USB controller to start pumping frames */
	uhci_Resources.Write2(UHCI_REG_USBCMD, UHCI_USBCMD_MAXP | UHCI_USBCMD_RS);
	delay(10);
	if (uhci_Resources.Read2(UHCI_REG_USBSTS) & UHCI_USBSTS_HCHALTED) {
		Printf("controller does not start");
		return RESULT_MAKE_FAILURE(ENODEV);
	}

	/* Hook up our interrupt handler and get it going */
	if (auto result = irq::Register((uintptr_t)res_irq, this, IRQ_TYPE_DEFAULT, *this); result.IsFailure())
		return result;
	uhci_Resources.Write2(UHCI_REG_USBINTR, UHCI_USBINTR_SPI | UHCI_USBINTR_IOC | UHCI_USBINTR_RI | UHCI_USBINTR_TOCRC);
	delay(10);

	return Result::Success();
}

Result
UHCI_HCD::Detach()
{
	panic("detach");
	return Result::Success();
}

void
UHCI_HCD::SetRootHub(usb::USBDevice& dev)
{
	uhci_RootHub = new uhci::RootHub(uhci_Resources, dev);
	Result result = uhci_RootHub->Initialize();
	(void)result; // XXX allow this function to fail
}

namespace {

struct UHCI_Driver : public Driver
{
	UHCI_Driver()
	 : Driver("uhci")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "pcibus";
	}

	Device* CreateDevice(const CreateDeviceProperties& cdp) override
	{
		auto class_res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PCI_ClassRev, 0);
		if (class_res == nullptr) /* XXX it's a bug if this happens */
			return nullptr;
		uint32_t classrev = class_res->r_Base;

		/* Generic UHCI USB device */
		if (PCI_CLASS(classrev) == PCI_CLASS_SERIAL && PCI_SUBCLASS(classrev) == PCI_SUBCLASS_USB && PCI_PROGINT(classrev) == 0)
			return new UHCI_HCD(cdp);
		return nullptr;
	}
};

} // unnamed namespace

REGISTER_DRIVER(UHCI_Driver)

} // namespace usb

/* vim:set ts=2 sw=2: */
