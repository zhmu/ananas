/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * The usbbus is the root of all USB devices; it is connected directly to the
 * HCD driver device, and takes care of things like device attachment, power
 * and bandwidth regulation and the like.
 *
 * Directly attached to the usbbus are the actual USB devices; the USB_DEVICE
 * structure is obtained as a resource and takes care of the USB details.
 *
 * For example:
 *
 * ohci0 <--- usbbus0
 *               |
 *               +-----< usbhub0
 *               |
 *               +-----< usb-keyboard0
 */
#include <ananas/util/list.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "usb-bus.h"
#include "usb-device.h"

namespace usb
{
    typedef util::List<Bus> BusList;

    namespace
    {
        Thread* usbbus_thread{};
        Semaphore usbbus_semaphore("usbbus-sem", 0);
        USBDeviceList usbbus_pendingqueue;
        Spinlock usbbus_spl_pendingqueue; /* protects usbbus_pendingqueue */
        BusList usbbus_busses;
        Mutex usbbus_mutex("usbbus"); /* protects usbbus_busses */
    }                                 // unnamed namespace

    void ScheduleAttach(USBDevice& usb_dev)
    {
        /* Add the device to our queue */
        {
            SpinlockGuard g(usbbus_spl_pendingqueue);
            usbbus_pendingqueue.push_back(usb_dev);
        }

        /* Wake up our thread */
        usbbus_semaphore.Signal();
    }

    Result Bus::Attach()
    {
        bus_NeedsExplore = true;

        /*
         * Create the root hub device; it will handle all our children - the HCD may
         * need to know about this as the root hub may be polling...
         */
        auto roothub = new USBDevice(*this, nullptr, 0, USB_DEVICE_FLAG_ROOT_HUB);
        d_Parent->GetUSBDeviceOperations()->SetRootHub(*roothub);
        ScheduleAttach(*roothub);

        /* Register ourselves within the big bus list */
        {
            MutexGuard g(usbbus_mutex);
            usbbus_busses.push_back(*this);
        }
        return Result::Success();
    }

    Result Bus::Detach()
    {
        panic("detach");
        return Result::Success();
    }

    int Bus::AllocateAddress()
    {
        /* XXX crude */
        static int cur_addr = 1;
        return cur_addr++;
    }

    void Bus::ScheduleExplore()
    {
        Lock();
        bus_NeedsExplore = true;
        Unlock();

        usbbus_semaphore.Signal();
    }

    /* Must be called with lock held! */
    void Bus::Explore()
    {
        AssertLocked();
        bus_NeedsExplore = false;

        for (auto& usb_dev : bus_devices) {
            Device* dev = usb_dev.ud_device;
            if (dev->GetUSBHubDeviceOperations() != nullptr)
                dev->GetUSBHubDeviceOperations()->HandleExplore();
        }
    }

    /* Must be called with lock held! */
    Result Bus::DetachHub(Hub& hub)
    {
        AssertLocked();

        for (auto it = bus_devices.begin(); it != bus_devices.end(); /* nothing */) {
            auto& usb_dev = *it;
            ++it;
            if (usb_dev.ud_hub != &hub)
                continue;

            Result err = usb_dev.Detach();
            (void)err; // XXX what to do in this case?
        }

        return Result::Success();
    }

    static void usb_bus_thread(void* unused)
    {
        /* Note that this thread is used for _all_ USB busses */
        while (1) {
            /* Wait until we have to wake up... */
            usbbus_semaphore.Wait();

            while (1) {
                /*
                 * See if any USB busses need to be explored; we do this first because
                 * exploring may trigger new devices to attach or old ones to remove.
                 */
                {
                    MutexGuard g(usbbus_mutex);
                    for (auto& bus : usbbus_busses) {
                        bus.Lock();
                        if (bus.bus_NeedsExplore)
                            bus.Explore();
                        bus.Unlock();
                    }
                }

                /* Handle attaching devices */
                USBDevice* usb_dev;
                {
                    SpinlockGuard g(usbbus_spl_pendingqueue);
                    if (usbbus_pendingqueue.empty())
                        break;
                    usb_dev = &usbbus_pendingqueue.front();
                    usbbus_pendingqueue.pop_front();
                }

                /*
                 * Note that attaching will block until completed, which is intentional
                 * as it ensures we will never attach more than one device in the system
                 * at any given time.
                 */
                Result result = usb_dev->Attach();
                KASSERT(
                    result.IsSuccess(), "cannot yet deal with failures %d", result.AsStatusCode());

                /* This worked; hook the device to the bus' device list */
                Bus& bus = usb_dev->ud_bus;
                bus.Lock();
                bus.bus_devices.push_back(*usb_dev);
                bus.Unlock();
            }
        }
    }

    namespace
    {
        struct USBBus_Driver : public Driver {
            USBBus_Driver() : Driver("usbbus") {}

            const char* GetBussesToProbeOn() const override { return "ohci,uhci"; }

            Device* CreateDevice(const CreateDeviceProperties& cdp) override
            {
                return new Bus(cdp);
            }
        };

        const init::OnInit initializeUSBBus(init::SubSystem::Device, init::Order::First, []() {
            /*
             * Create a kernel thread to handle USB device attachments. We use a thread for this
             * since we can only attach one at the same time.
             */
            if (auto result = kthread_alloc("usbbus", &usb_bus_thread, NULL, usbbus_thread);
                result.IsFailure())
                panic("cannot create usbbus thread");
            usbbus_thread->Resume();
        });

        const RegisterDriver<USBBus_Driver> registerDriver;

    } // unnamed namespace
} // namespace usb
