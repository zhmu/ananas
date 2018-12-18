/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __UHCI_HUB_H__
#define __UHCI_HUB_H__

namespace usb
{
    class Transfer;
    class USBDevice;
    class HCD_Resources;

    namespace uhci
    {
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
            void UpdateStatus();

          private:
            HCD_Resources& rh_Resources;
            USBDevice& rh_Device;

            unsigned int rh_numports = 0;
            bool rh_c_portreset = false;
            struct Thread rh_pollthread;
        };

    } // namespace uhci
} // namespace usb

#endif /* __UHCI_HUB_H__ */
