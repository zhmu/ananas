/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __OHCI_ROOTHUB_H__
#define __OHCI_ROOTHUB_H__

namespace usb
{
    class Transfer;
    class USBDevice;

    namespace ohci
    {
        class HCD_Resources;

        class RootHub
        {
          public:
            RootHub(HCD_Resources& hcdResources, USBDevice& usbDevice);
            Result Initialize();

            void SetUSBDevice(USBDevice& usbDevice);
            Result HandleTransfer(Transfer& xfer);
            void OnIRQ();

          protected:
            Result ControlTransfer(Transfer& xfer);
            void ProcessInterruptTransfers();
            static void ThreadWrapper(void* context) { (static_cast<RootHub*>(context))->Thread(); }
            void Thread();

          private:
            HCD_Resources& rh_Resources;
            USBDevice& rh_Device;

            int rh_numports;
            Semaphore rh_semaphore{0};
            struct Thread rh_pollthread;
            bool rh_pending_changes = true;
        };

    } // namespace ohci
} // namespace usb

#endif /* __OHCI_ROOTHUB_H__ */
