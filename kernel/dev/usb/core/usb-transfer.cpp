/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/list.h"
#include "kernel/mm.h"
#include "kernel/init.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "config.h"
#include "usb-bus.h"
#include "usb-core.h"
#include "usb-device.h"
#include "usb-transfer.h"

#if 0
#define DPRINTF kprintf
#else
#define DPRINTF(...)
#endif

namespace usb
{
    namespace
    {
        Thread usbtransfer_thread;
        Semaphore usbtransfer_sem("usbxfer-sem", 0);
        CompletedTransferList usbtransfer_complete;
        Spinlock usbtransfer_lock;

        inline Device& usbtransfer_get_hcd_device(Transfer& xfer)
        {
            /* All USB_DEVICE's are on usbbusX; the bus' parent is the HCD to use */
            return *xfer.t_device.ud_bus.d_Parent;
        }

        Result SetupTransfer(Transfer& xfer)
        {
            auto& hcd_dev = usbtransfer_get_hcd_device(xfer);
            KASSERT(hcd_dev.GetUSBDeviceOperations() != NULL, "transferring without usb transfer");
            return hcd_dev.GetUSBDeviceOperations()->SetupTransfer(xfer);
        }

    } // unnamed namespace

    Transfer* AllocateTransfer(USBDevice& usb_dev, int type, int flags, int endpt, size_t maxlen)
    {
        auto usb_xfer = new Transfer(usb_dev, type, flags);
        DPRINTF("usbtransfer_alloc: xfer=%x type %d\n", usb_xfer, type);
        usb_xfer->t_length = maxlen; /* transfer buffer length */
        usb_xfer->t_address = usb_dev.ud_address;
        usb_xfer->t_endpoint = endpt;

        if (auto result = SetupTransfer(*usb_xfer); result.IsFailure()) {
            kfree(usb_xfer);
            return nullptr;
        }
        return usb_xfer;
    }

    Result Transfer::Schedule()
    {
        /* All USB_DEVICE's are on usbbusX; the bus' parent is the HCD to use */
        auto& hcd_dev = usbtransfer_get_hcd_device(*this);

        /* Schedule the transfer; we are responsible for locking here */
        t_device.Lock();
        Result err = hcd_dev.GetUSBDeviceOperations()->ScheduleTransfer(*this);
        t_device.Unlock();
        return err;
    }

    Result Transfer::Cancel_Locked()
    {
        auto& hcd_dev = usbtransfer_get_hcd_device(*this);
        t_device.AssertLocked();

        // We have to ensure the transfer isn't in the completed queue; this would cause it to be
        // re-scheduled, which we must avoid XXX We already have the device lock here - is that
        // wise?
        {
            SpinlockGuard g(usbtransfer_lock);
            for (auto& xfer : usbtransfer_complete) {
                if (&xfer != this)
                    continue;

                usbtransfer_complete.remove(*this);
                break;
            }
        }

        // XXX how do we know the transfer thread wasn't handling _this_ completed transfer by here?
        return hcd_dev.GetUSBDeviceOperations()->CancelTransfer(*this);
    }

    Result Transfer::Cancel()
    {
        t_device.Lock();
        Result err = Cancel_Locked();
        t_device.Unlock();
        return err;
    }

    void FreeTransfer_Locked(Transfer& xfer)
    {
        auto& usb_dev = xfer.t_device;
        auto& hcd_dev = usbtransfer_get_hcd_device(xfer);
        usb_dev.AssertLocked();

        xfer.Cancel_Locked();
        hcd_dev.GetUSBDeviceOperations()->TearDownTransfer(xfer);
        delete &xfer;
    }

    void FreeTransfer(Transfer& xfer)
    {
        auto& usb_dev = xfer.t_device;

        DPRINTF("usbtransfer_free: xfer=%x type %d\n", &xfer, xfer->t_type);
        usb_dev.Lock();
        FreeTransfer_Locked(xfer);
        usb_dev.Unlock();
    }

    void Transfer::Complete_Locked()
    {
        t_device.AssertLocked();
        KASSERT(t_flags & TRANSFER_FLAG_PENDING, "completing transfer that isn't pending");

        /* Transfer is complete, so we can remove the pending flag */
        t_flags &= ~TRANSFER_FLAG_PENDING;
        t_device.ud_transfers.remove(*this);

        /*
         * This is generally called from interrupt context, so schedule a worker to
         * process the transfer; if the transfer doesn't have a callback function,
         * assume we'll just have to signal its semaphore.
         */
        if (t_callback != nullptr) {
            {
                SpinlockGuard g(usbtransfer_lock);
                usbtransfer_complete.push_back(*this);
            }

            usbtransfer_sem.Signal();
        } else {
            t_semaphore.Signal();
        }
    }

    void Transfer::Complete()
    {
        t_device.Lock();
        Complete_Locked();
        t_device.Unlock();
    }

    namespace
    {
        void transfer_thread(void* arg)
        {
            while (1) {
                /* Wait until there's something to report */
                usbtransfer_sem.Wait();

                /* Fetch an entry from the queue */
                while (1) {
                    Transfer* xfer;
                    {
                        SpinlockGuard g(usbtransfer_lock);
                        if (usbtransfer_complete.empty())
                            break;
                        xfer = &usbtransfer_complete.front();
                        usbtransfer_complete.pop_front();
                    }

                    /* And handle it */
                    KASSERT(
                        xfer->t_callback != nullptr, "xfer %p in completed list without callback?",
                        xfer);
                    xfer->t_callback(*xfer);

                    /*
                     * At this point, xfer may be freed, resubmitted or simply left lingering for
                     * the caller - we can't touch it at this point.
                     */
                }
            }
        }

        const init::OnInit initLaunch(init::SubSystem::Device, init::Order::First, []() {
            /* Create a kernel thread to handle USB completed messages */
            kthread_init(usbtransfer_thread, "usb-transfer", &transfer_thread, NULL);
            usbtransfer_thread.Resume();
        });

    } // unnamed namespace
} // namespace usb
