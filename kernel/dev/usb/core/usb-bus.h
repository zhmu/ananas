/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_USB_BUS_H__
#define __ANANAS_USB_BUS_H__

#include <ananas/util/list.h>
#include "kernel/device.h"

class Device;

namespace usb
{
    class USBDevice;
    class Hub;
    typedef util::List<USBDevice> USBDeviceList;

    /*
     * Single USB bus - directly connected to a HCD.
     */
    class Bus : public Device, private IDeviceOperations, public util::List<Bus>::NodePtr
    {
      public:
        using Device::Device;
        virtual ~Bus() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;

        bool bus_NeedsExplore = false;

        /* List of all USB devices on this bus */
        USBDeviceList bus_devices;

        int AllocateAddress();

        void ScheduleExplore();
        void Explore();
        Result DetachHub(Hub& hub);

        void Lock() { bus_mutex.Lock(); }

        void Unlock() { bus_mutex.Unlock(); }

        void AssertLocked() { bus_mutex.AssertLocked(); }

      private:
        /* Mutex protecting the bus */
        Mutex bus_mutex{"usbbus"};
    };

    void ScheduleAttach(USBDevice& usb_dev);

} // namespace usb

#endif /* __ANANAS_USB_BUS_H__ */
