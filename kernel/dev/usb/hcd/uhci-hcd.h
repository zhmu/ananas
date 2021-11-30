/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __UHCI_HCD_H__
#define __UHCI_HCD_H__

#include "kernel/irq.h"

#define UHCI_FRAMELIST_LEN (4096 / 4)
#define UHCI_NUM_INTERRUPT_QH 6 /* 1, 2, 4, 8, 16, 32ms queues */

namespace dma
{
    class Buffer;
}

namespace usb
{
    namespace uhci
    {
        struct HCD_ScheduledItem;

        typedef util::List<HCD_ScheduledItem> HCD_ScheduledItemList;

        struct HCD_TD;
        struct HCD_QH;

        class RootHub;

        class HCD_Resources
        {
          public:
            HCD_Resources() : res_io(0) {}

            HCD_Resources(uint32_t io) : res_io(io) {}

            inline uint16_t Read2(int reg) { return inw(res_io + reg); }

            inline uint32_t Read4(int reg) { return inl(res_io + reg); }

            inline void Write2(int reg, uint16_t value) { outw(res_io + reg, value); }

            inline void Write4(int reg, uint32_t value) { outl(res_io + reg, value); }

          private:
            /* I/O port */
            uint32_t res_io;
        };

    } // namespace uhci

    class UHCI_HCD : public Device,
                     private IDeviceOperations,
                     private IUSBDeviceOperations,
                     private irq::IIRQCallback
    {
      public:
        using Device::Device;
        virtual ~UHCI_HCD() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        IUSBDeviceOperations* GetUSBDeviceOperations() override { return this; }

        Result Attach() override;
        Result Detach() override;

        uhci::HCD_TD* AllocateTD();
        uhci::HCD_QH* AllocateQH();
        void FreeQH(uhci::HCD_QH* qh);

      protected:
        Result SetupTransfer(Transfer& xfer) override;
        Result TearDownTransfer(Transfer& xfer) override;
        Result CancelTransfer(Transfer& xfer) override;
        Result ScheduleTransfer(Transfer& xfer) override;
        void SetRootHub(usb::USBDevice& dev) override;

        Result ScheduleControlTransfer(Transfer& xfer);
        Result ScheduleInterruptTransfer(Transfer& xfer);

        void Dump();
        irq::IRQResult OnIRQ() override;

      private:
        dma::Buffer* uhci_framelist_buf = nullptr;
        uint32_t* uhci_framelist;

        uhci::HCD_Resources uhci_Resources;
        uhci::RootHub* uhci_RootHub = nullptr;

        /* Start of frame value */
        uint32_t uhci_sof_modify;
        /* Interrupt/control/bulk QH's */
        struct uhci::HCD_QH* uhci_qh_interrupt[UHCI_NUM_INTERRUPT_QH];
        struct uhci::HCD_QH* uhci_qh_ls_control;
        struct uhci::HCD_QH* uhci_qh_fs_control;
        struct uhci::HCD_QH* uhci_qh_bulk;
        /* Currently scheduled items */
        uhci::HCD_ScheduledItemList uhci_scheduled_items;
    };

} // namespace usb

#endif /* __UHCI_HCD_H__ */
