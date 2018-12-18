/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_OHCI_HCD_H__
#define __ANANAS_OHCI_HCD_H__

#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/irq.h"

#define OHCI_NUM_ED_LISTS 6 /* 1, 2, 4, 8, 16 and 32ms list */

namespace dma
{
    class Buffer;
}

namespace usb
{
    class Transfer;
    class USBDevice;

    namespace ohci
    {
        struct HCD_TD;
        struct HCD_ED;

        typedef util::List<HCD_ED> HCDEDList;

        class HCD_Resources
        {
          public:
            HCD_Resources() : ohci_membase(nullptr) {}

            HCD_Resources(uint8_t* membase) : ohci_membase(membase) {}

            inline void Write2(unsigned int reg, uint16_t value)
            {
                *(volatile uint16_t*)(ohci_membase + reg) = value;
            }

            inline void Write4(unsigned int reg, uint32_t value)
            {
                *(volatile uint32_t*)(ohci_membase + reg) = value;
            }

            inline uint16_t Read2(unsigned int reg)
            {
                return *(volatile uint16_t*)(ohci_membase + reg);
            }

            inline uint32_t Read4(unsigned int reg)
            {
                return *(volatile uint32_t*)(ohci_membase + reg);
            }

          private:
            volatile uint8_t* ohci_membase;
        };

        class RootHub;

    } // namespace ohci

    class OHCI_HCD : public Device,
                     private IDeviceOperations,
                     private IUSBDeviceOperations,
                     private irq::IHandler
    {
      public:
        using Device::Device;
        virtual ~OHCI_HCD() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        IUSBDeviceOperations* GetUSBDeviceOperations() override { return this; }

        Result Attach() override;
        Result Detach() override;
        void DebugDump() override { Dump(); }

        Result SetupTransfer(Transfer& xfer) override;
        Result TearDownTransfer(Transfer& xfer) override;
        Result ScheduleTransfer(Transfer& xfer) override;
        Result CancelTransfer(Transfer& xfer) override;
        void SetRootHub(usb::USBDevice& dev) override;

      protected:
        void Dump();
        Result Setup();

        void CreateTDs(Transfer& xfer);

      private:
        void Lock() { ohci_mtx.Lock(); }

        void Unlock() { ohci_mtx.Unlock(); }

        ohci::HCD_TD* AllocateTD();
        ohci::HCD_ED* AllocateED();
        ohci::HCD_ED* SetupED(Transfer& xfer);

        irq::IRQResult OnIRQ() override;

        dma::Buffer* ohci_hcca_buf = nullptr;
        OHCI_HCCA* ohci_hcca = nullptr;
        ohci::HCD_ED* ohci_interrupt_ed[OHCI_NUM_ED_LISTS];
        ohci::HCD_ED* ohci_control_ed = nullptr;
        ohci::HCD_ED* ohci_bulk_ed = nullptr;
        ohci::HCDEDList ohci_active_eds;
        Mutex ohci_mtx{"ohci"};

        ohci::HCD_Resources ohci_Resources;
        ohci::RootHub* ohci_RootHub = nullptr;
    };

} // namespace usb

#endif /* __ANANAS_OHCI_HCD_H__ */
