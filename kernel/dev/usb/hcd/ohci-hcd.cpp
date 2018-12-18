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
#include "kernel/dev/pci.h"
#include "kernel/dma.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/time.h"
#include "kernel/trace.h"
#include "../core/usb-core.h"
#include "../core/usb-device.h"
#include "../core/usb-transfer.h"
#include "ohci-reg.h"
#include "ohci-roothub.h"
#include "ohci-hcd.h"

#include "kernel-md/vm.h" // XXX for KVTOP, which we must remove

TRACE_SETUP;

namespace usb
{
    namespace ohci
    {
        // HCD_[ET]D must start with an OHCI_[ET]D struct - this is what the HCD
        // hardware uses! This means we cannot inherit from NodePtr
        struct HCD_TD {
            struct OHCI_TD td_td;
            dma::Buffer* td_buf = nullptr;
            uint32_t td_length;

            struct HCD_TD* td_next;
        };

        struct HCD_ED {
            struct OHCI_ED ed_ed;
            /* Virtual addresses of the TD chain */
            struct HCD_TD* ed_headtd;
            struct HCD_TD* ed_tailtd;
            /* Virtual addresses of the ED chain */
            struct HCD_ED* ed_preved;
            struct HCD_ED* ed_nexted;
            dma::Buffer* ed_buf = nullptr;
            Transfer* ed_xfer;

            /* Active queue fields; used by ohci_irq() to check all active transfers */
            util::List<HCD_ED>::Node np_NodePtr;
        };

        inline uint32_t GetPhysicalAddress(HCD_TD& td)
        {
            return td.td_buf->GetSegments().front().s_phys;
        }

        inline uint32_t GetPhysicalAddress(HCD_ED& ed)
        {
            return ed.ed_buf->GetSegments().front().s_phys;
        }

        void DumpTD(HCD_TD& td)
        {
            kprintf(
                "td %x -> flags %x (cc %d ec %d t %d di %d dp %d r %d) cbp %x nexttd %x be %x\n",
                GetPhysicalAddress(td), td.td_td.td_flags, (td.td_td.td_flags >> 28) & 0x1f,
                (td.td_td.td_flags >> 26) & 0x3, (td.td_td.td_flags >> 24) & 0x3,
                (td.td_td.td_flags >> 21) & 0x7, (td.td_td.td_flags >> 19) & 0x3,
                (td.td_td.td_flags >> 18) & 0x1, td.td_td.td_cbp, td.td_td.td_nexttd,
                td.td_td.td_be);
        }

        void DumpED(HCD_ED& ed)
        {
            kprintf(
                " ed %x -> flags %x (mps %d en %d fa %d dir %d %c%c%c) tailp %x headp %x (%c%c) "
                "nexted %x\n",
                GetPhysicalAddress(ed), ed.ed_ed.ed_flags, (ed.ed_ed.ed_flags >> 16) & 0x3ff,
                (ed.ed_ed.ed_flags >> 7) & 0xf, (ed.ed_ed.ed_flags & 0x7f),
                (ed.ed_ed.ed_flags >> 11) & 3, (ed.ed_ed.ed_flags & OHCI_ED_F) ? 'F' : '.',
                (ed.ed_ed.ed_flags & OHCI_ED_K) ? 'K' : '.',
                (ed.ed_ed.ed_flags & OHCI_ED_S) ? 'S' : '.', ed.ed_ed.ed_tailp, ed.ed_ed.ed_headp,
                (ed.ed_ed.ed_headp & OHCI_ED_HEADP_C) ? 'C' : '.',
                (ed.ed_ed.ed_headp & OHCI_ED_HEADP_H) ? 'H' : '.', ed.ed_ed.ed_nexted);

            for (HCD_TD* td = ed.ed_headtd; td != nullptr; td = td->td_next) {
                kprintf("  ");
                DumpTD(*td);
            }
        }

        void DumpEDChain(HCD_ED* ed)
        {
            for (HCD_ED* prev_ed = nullptr; ed != nullptr; prev_ed = ed, ed = ed->ed_nexted) {
                if (ed->ed_preved != prev_ed)
                    kprintf(">>> previous chain corrupt (%p, %p)\n", ed->ed_preved, prev_ed);
                DumpED(*ed);
            }
        }

        void FreeTD(HCD_TD* td)
        {
            delete td->td_buf;
            td->td_buf = nullptr;
        }

        void FreeED(HCD_ED* ed)
        {
            for (HCD_TD* td = ed->ed_headtd; td != nullptr; /* nothing */) {
                ohci::HCD_TD* next_td = td->td_next;
                FreeTD(td);
                td = next_td;
            }

            delete ed->ed_buf;
            ed->ed_buf = nullptr;
        }

        void SetTDNext(HCD_TD& td, HCD_TD& next)
        {
            td.td_td.td_nexttd = GetPhysicalAddress(next);
            td.td_next = &next;
        }

        /* Enqueues 'ed' *after* 'parent' - assumes ED is skipped */
        void EnqueueED(HCD_ED& parent, HCD_ED& ed)
        {
            KASSERT(ed.ed_ed.ed_flags & OHCI_ED_K, "adding ed %p that isn't sKip");

            /* Virtual addresses part */
            if (parent.ed_nexted != nullptr)
                parent.ed_nexted->ed_preved = &ed;
            ed.ed_nexted = parent.ed_nexted;
            parent.ed_nexted = &ed;
            ed.ed_preved = &parent;

            /* OHCI part */
            ed.ed_ed.ed_nexted = parent.ed_ed.ed_nexted;
            parent.ed_ed.ed_nexted = GetPhysicalAddress(ed);
        }

        void FreeTDsFromTransfer(Transfer& xfer)
        {
            auto ed = static_cast<struct ohci::HCD_ED*>(xfer.t_hcd);
            if (ed == nullptr)
                return; /* nothing to free */

            /*
             * We'll free all TD's here except the tail one; this basically undoes the work done by
             * ohci_create_tds().
             */
            for (HCD_TD* td = ed->ed_headtd; td != nullptr && td != ed->ed_tailtd; /* nothing */) {
                HCD_TD* next_td = td->td_next;
                FreeTD(td);
                td = next_td;
            }
            ed->ed_headtd = nullptr;
        }

    } // namespace ohci

    void OHCI_HCD::Dump()
    {
        Printf(
            "hcca %x -> fnum %d dh %x", ohci_Resources.Read4(OHCI_HCHCCA),
            ohci_hcca->hcca_framenumber, ohci_hcca->hcca_donehead);
        Printf(
            "control %x cmdstat %x intstat %x intena %x intdis %x",
            ohci_Resources.Read4(OHCI_HCCONTROL), ohci_Resources.Read4(OHCI_HCCOMMANDSTATUS),
            ohci_Resources.Read4(OHCI_HCINTERRUPTSTATUS),
            ohci_Resources.Read4(OHCI_HCINTERRUPTENABLE),
            ohci_Resources.Read4(OHCI_HCINTERRUPTDISABLE));
        Printf(
            "period_cured %x ctrlhead %x ctrlcur %x bulkhead %x bulkcur %x",
            ohci_Resources.Read4(OHCI_HCPERIODCURRENTED),
            ohci_Resources.Read4(OHCI_HCCONTROLHEADED),
            ohci_Resources.Read4(OHCI_HCCONTROLCURRENTED), ohci_Resources.Read4(OHCI_HCBULKHEADED),
            ohci_Resources.Read4(OHCI_HCBULKCURRENTED));
        Printf(
            "rhda %x rhdb %x rhstatus %x rhps[0] %x rhps[1] %x rphs[2] %x",
            ohci_Resources.Read4(OHCI_HCRHDESCRIPTORA), ohci_Resources.Read4(OHCI_HCRHDESCRIPTORB),
            ohci_Resources.Read4(OHCI_HCRHSTATUS), ohci_Resources.Read4(OHCI_HCRHPORTSTATUSx),
            ohci_Resources.Read4(OHCI_HCRHPORTSTATUSx + 4),
            ohci_Resources.Read4(OHCI_HCRHPORTSTATUSx + 8));

        kprintf("** dumping control chain\n");
        ohci::DumpEDChain(ohci_control_ed);

        kprintf("** dumping bulk chain\n");
        ohci::DumpEDChain(ohci_bulk_ed);

        if (!ohci_active_eds.empty()) {
            kprintf("** dumping active EDs\n");
            for (auto& ed : ohci_active_eds) {
                kprintf("ed %p -> xfer %p\n", &ed, ed.ed_xfer);
                ohci::DumpED(ed);
            }
        }

        kprintf("** periodic list\n");
        for (unsigned int n = 0; n < OHCI_NUM_ED_LISTS; n++) {
            kprintf("> %d ms list\n", 1 << n);
            ohci::DumpEDChain(ohci_interrupt_ed[n]);
        }
    }

    irq::IRQResult OHCI_HCD::OnIRQ()
    {
        /*
         * Obtain the interrupt status. Note that donehead is funky; bit 0 indicates
         * whethere standard processing is required (if not, this interrupt only
         * indicates hcca_donehead was updated)
         */
        uint32_t is = 0;
        uint32_t dh = ohci_hcca->hcca_donehead;
        if (dh != 0) {
            is = OHCI_IS_WDH;
            if (dh & 1)
                is |= ohci_Resources.Read4(OHCI_HCINTERRUPTSTATUS) &
                      ohci_Resources.Read4(OHCI_HCINTERRUPTENABLE);
        } else {
            is = ohci_Resources.Read4(OHCI_HCINTERRUPTSTATUS) &
                 ohci_Resources.Read4(OHCI_HCINTERRUPTENABLE);
        }
        if (is == 0) {
            /* Not my interrupt; ignore */
            return irq::IRQResult::Ignored;
        }

        if (is & OHCI_IS_WDH) {
            /*
             * Done queue has been updated; need to walk through all our scheduled items.
             */
            Lock();
            if (!ohci_active_eds.empty()) {
                for (auto& ed : ohci_active_eds) {
                    if (ed.ed_ed.ed_flags & OHCI_ED_K)
                        continue;
                    /*
                     * Transfer is done if the ED's head and tail pointer match (minus the
                     * extra flag fields which only appear in the head field) or if the TD
                     * was halted (which the HC should do in case of error)
                     */
                    if ((ed.ed_ed.ed_tailp != (ed.ed_ed.ed_headp & ~0xf)) /* headp != tailp */ &&
                        (ed.ed_ed.ed_headp & OHCI_ED_HEADP_H) == 0) /* not halted */ {
                        continue;
                    }

                    /* Walk through all TD's and determine the length plus the status */
                    size_t transferred = 0;
                    int status = OHCI_TD_CC_NOERROR;
                    for (struct ohci::HCD_TD* td = ed.ed_headtd; td != nullptr; td = td->td_next) {
                        if (td->td_td.td_cbp == 0)
                            transferred += td->td_length; /* full TD */
                        else
                            transferred += td->td_length - (td->td_td.td_be - td->td_td.td_cbp +
                                                            1); /* partial TD */
                        if (status != OHCI_TD_CC_NOERROR)
                            status = OHCI_TD_CC(td->td_td.td_flags);
                    }

                    Transfer& xfer = *ed.ed_xfer;
                    xfer.t_data_toggle = (ed.ed_ed.ed_headp & OHCI_ED_HEADP_C) != 0;
                    xfer.t_result_length = transferred;
                    if (status != OHCI_TD_CC_NOERROR)
                        xfer.t_flags |= TRANSFER_FLAG_ERROR;
                    xfer.Complete();

                    /* Skip ED now, it's processed */
                    ed.ed_ed.ed_flags |= OHCI_ED_K;
                }
            } else {
                kprintf("WDH without eds?!\n");
                kprintf("donehead=%p\n", ohci_hcca->hcca_donehead);
            }
            Unlock();

            ohci_hcca->hcca_donehead = 0; /* acknowledge donehead */
            ohci_Resources.Write4(OHCI_HCINTERRUPTSTATUS, OHCI_IS_WDH);
        }
        if (is & OHCI_IS_SO) {
            Printf("scheduling overrun!");
            Dump();
            panic("XXX scheduling overrun");
            ohci_Resources.Write4(OHCI_HCINTERRUPTSTATUS, OHCI_IS_SO);
        }

        if (is & OHCI_IS_FNO) {
            Printf("frame num overflow!");
            Dump();

            ohci_Resources.Write4(OHCI_HCINTERRUPTSTATUS, OHCI_IS_FNO);
        }

        if (is & OHCI_IS_RHSC) {
            if (ohci_RootHub != nullptr)
                ohci_RootHub->OnIRQ();
            ohci_Resources.Write4(OHCI_HCINTERRUPTSTATUS, OHCI_IS_RHSC);
        }

        /* If we got anything unexpected, warn and disable them */
        is &= ~(OHCI_IS_WDH | OHCI_IS_SO | OHCI_IS_FNO | OHCI_IS_RHSC);
        if (is != 0) {
            Printf("disabling excessive interrupts %x", is);
            ohci_Resources.Write4(OHCI_HCINTERRUPTDISABLE, is);
        }
        return irq::IRQResult::Processed;
    }

    ohci::HCD_TD* OHCI_HCD::AllocateTD()
    {
        dma::Buffer* dma_buffer;
        if (auto result = d_DMA_tag->AllocateBuffer(sizeof(struct ohci::HCD_TD), dma_buffer);
            result.IsFailure())
            return nullptr;

        auto td = static_cast<struct ohci::HCD_TD*>(dma_buffer->GetSegments().front().s_virt);
        memset(td, 0, sizeof(struct ohci::HCD_TD));
        td->td_buf = dma_buffer;
        return td;
    }

    ohci::HCD_ED* OHCI_HCD::AllocateED()
    {
        dma::Buffer* dma_buffer;
        if (auto result = d_DMA_tag->AllocateBuffer(sizeof(struct ohci::HCD_ED), dma_buffer);
            result.IsFailure())
            return nullptr;

        auto ed = static_cast<struct ohci::HCD_ED*>(dma_buffer->GetSegments().front().s_virt);
        memset(ed, 0, sizeof(struct ohci::HCD_ED));
        ed->ed_buf = dma_buffer;
        return ed;
    }

    ohci::HCD_ED* OHCI_HCD::SetupED(Transfer& xfer)
    {
        USBDevice& usb_dev = xfer.t_device;
        int is_ls = (usb_dev.ud_flags & USB_DEVICE_FLAG_LOW_SPEED) != 0;
        int max_packet_sz = usb_dev.ud_max_packet_sz0; /* XXX this is wrong for non-control */

        KASSERT(
            xfer.t_type != TRANSFER_TYPE_INTERRUPT || (xfer.t_flags & TRANSFER_FLAG_DATA),
            "interrupt transfer without data?");

        /* If this is a root hub device transfer, we don't actually have to create any TD/ED's as we
         * handle it internally */
        if (usb_dev.ud_flags & USB_DEVICE_FLAG_ROOT_HUB)
            return nullptr;

        /* Construct an endpoint descriptor for this device */
        ohci::HCD_ED* ed = AllocateED();
        KASSERT(ed != nullptr, "out of memory when allocating ed");
        ed->ed_xfer = &xfer;
        ed->ed_ed.ed_flags = OHCI_ED_K /* sKip makes the ED inactive */ |
                             OHCI_ED_FA(xfer.t_address) | OHCI_ED_EN(xfer.t_endpoint) |
                             OHCI_ED_D(OHCI_ED_D_TD) | OHCI_ED_MPS(max_packet_sz) |
                             (is_ls ? OHCI_ED_S : 0);

        /* Every transfer must end to a dummy TD, which we'll also setup here... */
        ed->ed_tailtd = AllocateTD();

        /* ... and link to the ED */
        ed->ed_ed.ed_headp = GetPhysicalAddress(*ed->ed_tailtd);
        ed->ed_ed.ed_tailp = GetPhysicalAddress(*ed->ed_tailtd);
        ed->ed_headtd = nullptr;

        /* Finally, hook it up to the correct queue */
        switch (xfer.t_type) {
            case TRANSFER_TYPE_CONTROL:
                ohci::EnqueueED(*ohci_control_ed, *ed);
                break;
            case TRANSFER_TYPE_BULK:
                ohci::EnqueueED(*ohci_bulk_ed, *ed);
                break;
            case TRANSFER_TYPE_INTERRUPT: {
                int ed_list = 0; /* XXX */
                ohci::EnqueueED(*ohci_interrupt_ed[ed_list], *ed);
                break;
            }
            default:
                panic("implement type %d", xfer.t_type);
        }
        return ed;
    }

    Result OHCI_HCD::SetupTransfer(Transfer& xfer)
    {
        auto& usb_dev = xfer.t_device;

        /* If this is the root hub, there's nothing to set up */
        if (usb_dev.ud_flags & USB_DEVICE_FLAG_ROOT_HUB)
            return Result::Success();

        /*
         * Create the endpoint descriptor; this is where we'll chain transfers to. We
         * need the ED anyway and creating it here ensures we won't have to re-do all
         * that work when we're doing re-scheduling transfers.
         */
        ohci::HCD_ED* ed = SetupED(xfer);
        xfer.t_hcd = ed;

        /* Hook the ED to the queue; it won't do anything yet */
        Lock();
        ohci_active_eds.push_back(*ed);
        Unlock();
        return Result::Success();
    }

    Result OHCI_HCD::TearDownTransfer(Transfer& xfer)
    {
        auto& usb_dev = xfer.t_device;
        usb_dev.AssertLocked();

        auto ed = static_cast<ohci::HCD_ED*>(xfer.t_hcd);
        if (ed == nullptr)
            return Result::Success();

        /* First of all, ensure the ED is marked as sKip in hopes the HC won't touch it */
        ed->ed_ed.ed_flags |= OHCI_ED_K;

        /* Removes ourselves from the hardware ED chain */
        KASSERT(ed->ed_preved != nullptr, "removing head ed %p", ed);
        ed->ed_preved->ed_ed.ed_nexted = ed->ed_ed.ed_nexted;
        ed->ed_preved->ed_nexted = ed->ed_nexted;
        if (ed->ed_nexted != nullptr)
            ed->ed_nexted->ed_preved = ed->ed_preved;

        /* And from our own administration */
        Lock();
        ohci_active_eds.remove(*ed);
        Unlock();

        /* Finally, we can kill the ED itself XXX We should ensure it's no longer used */
        FreeED(ed);
        xfer.t_hcd = nullptr;
        return Result::Success();
    }

    void OHCI_HCD::CreateTDs(Transfer& xfer)
    {
        auto ed = static_cast<struct ohci::HCD_ED*>(xfer.t_hcd);
        bool is_read = (xfer.t_flags & TRANSFER_FLAG_READ) != 0;

        KASSERT(ed != nullptr, "ohci_create_tds() without ed?");
        KASSERT(ed->ed_headtd == nullptr, "ohci_create_tds() with TD's");

        /* Construct the SETUP/HANDSHAKE transfer descriptors, if it's a control transfer */
        ohci::HCD_TD* td_setup = nullptr;
        ohci::HCD_TD* td_handshake = nullptr;
        if (xfer.t_type == TRANSFER_TYPE_CONTROL) {
            /* Control messages have fixed DATA0/1 types */
            td_setup = AllocateTD();
            td_setup->td_td.td_flags = OHCI_TD_DP(OHCI_TD_DP_SETUP) | OHCI_TD_DI(OHCI_TD_DI_NONE) |
                                       OHCI_TD_T(2) /* DATA0 */;
            td_setup->td_td.td_cbp = KVTOP((addr_t)&xfer.t_control_req); /* XXX64 TODO */
            td_setup->td_td.td_be = td_setup->td_td.td_cbp + sizeof(struct USB_CONTROL_REQUEST) - 1;

            td_handshake = AllocateTD();
            td_handshake->td_td.td_flags = OHCI_TD_DI(OHCI_TD_DI_IMMEDIATE) |
                                           OHCI_TD_T(3) /* DATA1 */ |
                                           OHCI_TD_DP(is_read ? OHCI_TD_DP_OUT : OHCI_TD_DP_IN);
        }

        /* Construct the DATA transfer descriptor */
        ohci::HCD_TD* td_data = nullptr;
        if (xfer.t_flags & TRANSFER_FLAG_DATA) {
            td_data = AllocateTD();
            /*
             * Note that we don't use OHCI_TD_T() here; default is to invert the parent
             * and for non-control transfers this will be td_head which we override
             * anyway.
             */
            td_data->td_td.td_flags = OHCI_TD_R | OHCI_TD_DI(OHCI_TD_DI_NONE) |
                                      OHCI_TD_DP(is_read ? OHCI_TD_DP_IN : OHCI_TD_DP_OUT);
            td_data->td_td.td_cbp = KVTOP((addr_t)&xfer.t_data[0]); /* XXX64 */
            td_data->td_length = xfer.t_length;
            td_data->td_td.td_be = td_data->td_td.td_cbp + td_data->td_length - 1;
        }

        /* Build the chain of TD's */
        ohci::HCD_TD* td_head = nullptr;
        switch (xfer.t_type) {
            case TRANSFER_TYPE_CONTROL:
                /* Control transfer: setup -> (data) -> handshake -> tail */
                SetTDNext(*td_handshake, *ed->ed_tailtd);
                if (td_data != nullptr) {
                    SetTDNext(*td_data, *td_handshake);
                    SetTDNext(*td_setup, *td_data);
                } else {
                    SetTDNext(*td_setup, *td_handshake);
                }

                td_head = td_setup;
                break;
            case TRANSFER_TYPE_INTERRUPT:
                /* Interrupt transfer: data -> tail */
                SetTDNext(*td_data, *ed->ed_tailtd);
                td_head = td_data;

                /* XXX kludge: ensure we'll get an interrupt if the transfer succeeds */
                td_data->td_td.td_flags &= ~OHCI_TD_DI(OHCI_TD_DI_MASK);
                td_data->td_td.td_flags |= OHCI_TD_DI(OHCI_TD_DI_IMMEDIATE);
                break;
            case TRANSFER_TYPE_BULK:
                /* Bulk transfer: data -> tail */
                SetTDNext(*td_data, *ed->ed_tailtd);
                td_head = td_data;

                /* XXX kludge: ensure we'll get an interrupt if the transfer succeeds */
                td_data->td_td.td_flags &= ~OHCI_TD_DI(OHCI_TD_DI_MASK);
                td_data->td_td.td_flags |= OHCI_TD_DI(OHCI_TD_DI_IMMEDIATE);
                break;
            default:
                panic("unsupported transfer type %d", xfer.t_type);
        }

        /*
         * Alter the data toggle bit of the first TD; everything else will just
         * toggle it (or plainly override it as needed)
         */
        if (xfer.t_type != TRANSFER_TYPE_CONTROL) {
            td_head->td_td.td_flags &= ~OHCI_TD_T(3);
            if (xfer.t_data_toggle)
                td_head->td_td.td_flags |= OHCI_TD_T(3);
            else
                td_head->td_td.td_flags |= OHCI_TD_T(2);
        }

        /* Now hook our transfer to the ED... */
        ed->ed_headtd = td_head;
        ed->ed_ed.ed_headp = GetPhysicalAddress(*td_head);

        /* ...and mark it as active as it's good to go now */
        ed->ed_ed.ed_flags &= ~OHCI_ED_K;
    }

    /* We assume the USB device and transfer are locked here */
    Result OHCI_HCD::ScheduleTransfer(Transfer& xfer)
    {
        auto& usb_dev = xfer.t_device;
        usb_dev.AssertLocked();

        /*
         * Add the transfer to our pending list; this is done so we can cancel any
         * pending transfers when a device is removed, for example.
         */
        KASSERT(
            (xfer.t_flags & TRANSFER_FLAG_PENDING) == 0,
            "scheduling transfer that is already pending (%x)", xfer.t_flags);
        xfer.t_flags |= TRANSFER_FLAG_PENDING;
        usb_dev.ud_transfers.push_back(xfer);

        /* If this is the root hub, immediately transfer the request to it */
        if (usb_dev.ud_flags & USB_DEVICE_FLAG_ROOT_HUB)
            return ohci_RootHub->HandleTransfer(xfer);

        /* XXX We should re-cycle them instead... */
        ohci::FreeTDsFromTransfer(xfer);

        /*
         * Create the TD's that make up this transfer - this'll hook them to the ED
         * we created in ohci_setup_transfer().
         */
        CreateTDs(xfer);

        /* Kick the appropriate queue, if needed */
        switch (xfer.t_type) {
            case TRANSFER_TYPE_CONTROL:
                ohci_Resources.Write4(OHCI_HCCOMMANDSTATUS, OHCI_CS_CLF);
                break;
            case TRANSFER_TYPE_BULK:
                ohci_Resources.Write4(OHCI_HCCOMMANDSTATUS, OHCI_CS_BLF);
                break;
            case TRANSFER_TYPE_INTERRUPT:
                break;
            default:
                panic("implement type %d", xfer.t_type);
        }

        return Result::Success();
    }

    /* We assume the USB device and transfer are locked here */
    Result OHCI_HCD::CancelTransfer(Transfer& xfer)
    {
        auto& usb_dev = xfer.t_device;
        usb_dev.AssertLocked();

        if (xfer.t_flags & TRANSFER_FLAG_PENDING) {
            xfer.t_flags &= ~TRANSFER_FLAG_PENDING;
            usb_dev.ud_transfers.remove(xfer);
        }

        /* XXX we should see if we're still running it */
        ohci::FreeTDsFromTransfer(xfer);

        return Result::Success();
    }

    Result OHCI_HCD::Setup()
    {
        /* Allocate and initialize the HCCA structure */
        if (auto result = d_DMA_tag->AllocateBuffer(sizeof(struct OHCI_HCCA), ohci_hcca_buf);
            result.IsFailure())
            return result;
        ohci_hcca = static_cast<struct OHCI_HCCA*>(ohci_hcca_buf->GetSegments().front().s_virt);
        memset(ohci_hcca, 0, sizeof(struct OHCI_HCCA));

        /*
         * Create our lists; every new list should contain a back pointer to the
         * previous one - we use powers of two for each interval, so every Xms is
         * also in every 2*X ms.
         */
        for (unsigned int n = 0; n < OHCI_NUM_ED_LISTS; n++) {
            ohci::HCD_ED* ed = AllocateED();
            KASSERT(ed != nullptr, "out of eds");
            ohci_interrupt_ed[n] = ed;
            ed->ed_ed.ed_flags = OHCI_ED_K;
            if (n > 0)
                ed->ed_ed.ed_nexted =
                    ohci_interrupt_ed[n - 1]->ed_buf->GetSegments().front().s_phys;
            else
                ed->ed_ed.ed_nexted = 0;
        }

        /* Hook up the lists to the interrupt transfer table XXX find formula for this */
        const unsigned int tables[32] = {0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4,
                                         0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5};
        for (unsigned int n = 0; n < 32; n++) {
            ohci_hcca->hcca_inttable[n] =
                ohci_interrupt_ed[tables[n]]->ed_buf->GetSegments().front().s_phys;
        }

        /* Allocate a control/bulk transfer ED */
        ohci_control_ed = AllocateED();
        ohci_bulk_ed = AllocateED();
        KASSERT(ohci_control_ed != nullptr && ohci_bulk_ed != nullptr, "out of ed");

        ohci_control_ed->ed_ed.ed_flags = OHCI_ED_K;
        ohci_bulk_ed->ed_ed.ed_flags = OHCI_ED_K;
        return Result::Success();
    }

    Result OHCI_HCD::Attach()
    {
        void* res_mem = d_ResourceSet.AllocateResource(Resource::RT_Memory, 4096);
        if (res_mem == nullptr)
            return RESULT_MAKE_FAILURE(ENODEV);
        pci_enable_busmaster(*this, 1);

        /* See if the revision makes sense; if not, we can't attach to this */
        uint32_t rev = *(volatile uint32_t*)((char*)res_mem + OHCI_HCREVISION);
        if (OHCI_REVISION(rev) != 0x10) {
            Printf("unsupported revision 0x%x", OHCI_REVISION(rev));
            return RESULT_MAKE_FAILURE(ENODEV);
        }

        /* Allocate DMA tags */
        {
            using namespace dma::limits;
            d_DMA_tag = dma::CreateTag(
                d_Parent->d_DMA_tag, anyAlignment, minAddr, max32BitAddr, 1, maxSegmentSize);
        }

        ohci_Resources = ohci::HCD_Resources(static_cast<uint8_t*>(res_mem));

        /* Set up the interrupt handler */
        if (auto result = GetBusDeviceOperations().AllocateIRQ(*this, 0, *this); result.IsFailure())
            return result;

        /* Initialize the structures */
        if (auto result = Setup(); result.IsFailure())
            return result;

        if (ohci_Resources.Read4(OHCI_HCCONTROL) & OHCI_CONTROL_IR) {
            /* Controller is in SMM mode - we need to ask it to stop doing that */
            ohci_Resources.Write4(OHCI_HCCOMMANDSTATUS, OHCI_CS_OCR);
            int n = 5000;
            while (n > 0 && ohci_Resources.Read4(OHCI_HCCONTROL) & OHCI_CONTROL_IR)
                n--; /* XXX kludge; should use a real timeout mechanism */
            if (n == 0) {
                Printf("stuck in smm, giving up");
                return RESULT_MAKE_FAILURE(ENODEV);
            }
        }

        /* Kludge: force reset state */
        ohci_Resources.Write4(OHCI_HCCONTROL, OHCI_CONTROL_HCFS(OHCI_HCFS_USBRESET));

        /* Save contents of 'frame interval' and reset the HC */
        uint32_t fi = OHCI_FM_FI(ohci_Resources.Read4(OHCI_HCFMINTERVAL));
        ohci_Resources.Write4(OHCI_HCCOMMANDSTATUS, OHCI_CS_HCR);
        int n = 5000;
        while (n > 0) {
            delay(1);
            if ((ohci_Resources.Read4(OHCI_HCCOMMANDSTATUS) & OHCI_CS_HCR) == 0)
                break;
            n--; /* XXX kludge; should use a real timeout mechanism */
        }
        if (n == 0) {
            Printf("stuck in reset, giving up");
            return RESULT_MAKE_FAILURE(ENODEV);
        }
        /* Now in USBSUSPEND state -> we have 2ms to continue */

        /* ohci_Resources.Write addresses of our buffers */
        ohci_Resources.Write4(OHCI_HCHCCA, ohci_hcca_buf->GetSegments().front().s_phys);
        ohci_Resources.Write4(OHCI_HCCONTROLHEADED, ohci::GetPhysicalAddress(*ohci_control_ed));
        ohci_Resources.Write4(OHCI_HCBULKHEADED, ohci::GetPhysicalAddress(*ohci_bulk_ed));

        /* Enable interrupts */
        ohci_Resources.Write4(
            OHCI_HCINTERRUPTDISABLE, (OHCI_ID_SO | OHCI_ID_WDH | OHCI_ID_SF | OHCI_ID_RD |
                                      OHCI_ID_UE | OHCI_ID_FNO | OHCI_ID_RHSC | OHCI_ID_OC));
        int ints_enabled =
            (OHCI_IE_SO | OHCI_IE_WDH | OHCI_IE_RD | OHCI_IE_UE | OHCI_IE_RHSC | OHCI_IE_OC);
        ohci_Resources.Write4(OHCI_HCINTERRUPTENABLE, ints_enabled | OHCI_IE_MIE);

        /* Start sending USB frames */
        uint32_t c = ohci_Resources.Read4(OHCI_HCCONTROL);
        c &=
            ~(OHCI_CONTROL_CBSR(OHCI_CBSR_MASK) | OHCI_CONTROL_HCFS(OHCI_HCFS_MASK) |
              OHCI_CONTROL_PLE | OHCI_CONTROL_IE | OHCI_CONTROL_CLE | OHCI_CONTROL_BLE |
              OHCI_CONTROL_IR);
        c |= OHCI_CONTROL_PLE | OHCI_CONTROL_IE | OHCI_CONTROL_CLE | OHCI_CONTROL_BLE;
        c |= OHCI_CONTROL_CBSR(OHCI_CBSR_4_1) | OHCI_CONTROL_HCFS(OHCI_HCFS_USBOPERATIONAL);
        ohci_Resources.Write4(OHCI_HCCONTROL, c);

        /* State is now OPERATIONAL -> we can write interval/periodic start now */
        uint32_t fm = (ohci_Resources.Read4(OHCI_HCFMINTERVAL) ^ OHCI_FM_FIT); /* invert FIT bit */
        fm |= OHCI_FM_FSMPS(
            ((fi - OHCI_FM_MAX_OVERHEAD) * 6) / 7); /* calculate FSLargestDataPacket */
        fm |= fi;                                   /* restore frame interval */
        ohci_Resources.Write4(OHCI_HCFMINTERVAL, fm);
        uint32_t ps = (OHCI_FM_FI(fi) * 9) / 10;
        ohci_Resources.Write4(OHCI_HCPERIODICSTART, ps);

        /* 'Fiddle' with roothub registers to awaken it */
        uint32_t a = ohci_Resources.Read4(OHCI_HCRHDESCRIPTORA);
        ohci_Resources.Write4(OHCI_HCRHDESCRIPTORA, a | OHCI_RHDA_NOCP);
        ohci_Resources.Write4(OHCI_HCRHSTATUS, OHCI_RHS_LPSC); /* set global power */
        delay(10);
        ohci_Resources.Write4(OHCI_HCRHDESCRIPTORA, a);

        return Result::Success();
    }

    Result OHCI_HCD::Detach()
    {
        panic("Detach");
        return Result::Success();
    }

    void OHCI_HCD::SetRootHub(usb::USBDevice& dev)
    {
        KASSERT(ohci_RootHub == nullptr, "roothub is already set");
        ohci_RootHub = new ohci::RootHub(ohci_Resources, dev);
        auto result = ohci_RootHub->Initialize();
        (void)result; // XXX check error
    }

    namespace
    {
        struct OHCI_Driver : public Driver {
            OHCI_Driver() : Driver("ohci") {}

            const char* GetBussesToProbeOn() const override { return "pcibus"; }

            Device* CreateDevice(const CreateDeviceProperties& cdp) override
            {
                auto class_res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PCI_ClassRev, 0);
                if (class_res == nullptr) /* XXX it's a bug if this happens */
                    return nullptr;
                uint32_t classrev = class_res->r_Base;

                /* Generic OHCI USB device */
                if (PCI_CLASS(classrev) == PCI_CLASS_SERIAL &&
                    PCI_SUBCLASS(classrev) == PCI_SUBCLASS_USB && PCI_PROGINT(classrev) == 0x10)
                    return new OHCI_HCD(cdp);
                return nullptr;
            }
        };

        const RegisterDriver<OHCI_Driver> registerDriver;

    } // unnamed namespace
} // namespace usb

/* vim:set ts=2 sw=2: */
