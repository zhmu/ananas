/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/list.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "config.h"
#include "usb-bus.h"
#include "usb-core.h"
#include "usb-device.h"
#include "usb-transfer.h"
#include "../device/usb-hub.h"
#include <cstdint>

namespace usb
{
    USBDevice::USBDevice(Bus& bus, Hub* hub, int hub_port, int flags)
        : ud_bus(bus), ud_hub(hub), ud_port(hub_port), ud_flags(flags)
    {
        memset(&ud_interface, 0, sizeof(ud_interface));
        memset(&ud_descr_device, 0, sizeof(ud_descr_device));
    }

    USBDevice::~USBDevice()
    {
        delete ud_device; // XXX likely we need to do something else here...
    }

    /*
     * Attaches a single USB device (which hasn't got an address or anything yet)
     *
     * Should _only_ be called by the usb-bus thread!
     */
    Result USBDevice::Attach()
    {
        /*
         * First step is to reset the port - we do this here to prevent multiple ports from being
         * reset.
         *
         * Note that usb_hub can be NULL if we're attaching the root hub itself.
         */
        if (ud_hub != nullptr) {
            if (auto result = ud_hub->ResetPort(ud_port); result.IsFailure())
                return result;
        }

        /*
         * Obtain the first 8 bytes of the device descriptor; this tells us how how
         * large the control endpoint requests can be.
         */
        {
            size_t len = 8;
            if (auto result = PerformControlTransfer(
                    USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE,
                    USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_DEVICE, 0), 0,
                    &ud_descr_device, &len, false);
                result.IsFailure())
                return result;

            // Store the maximum endpoint 0 packet size
            ud_max_packet_sz0 = ud_descr_device.dev_maxsize0;
        }

        // Address phase
        {
            /* Construct a device address */
            int dev_address = ud_bus.AllocateAddress();
            if (dev_address <= 0) {
                kprintf("out of addresses, aborting attachment!\n");
                return Result::Failure(ENODEV); // XXX maybe not the best error to give
            }

            /* Assign the device a logical address */
            if (auto result = PerformControlTransfer(
                    USB_CONTROL_REQUEST_SET_ADDRESS, USB_CONTROL_RECIPIENT_DEVICE,
                    USB_CONTROL_TYPE_STANDARD, dev_address, 0, NULL, NULL, true);
                result.IsFailure())
                return result;

            /*
             * Address configured - we could attach more things in parallel from now on,
             * but this only complicates things, so let's not go there...
             */
            ud_address = dev_address;
        }

        /* Now, obtain the entire device descriptor */
        {
            size_t len = sizeof(ud_descr_device);
            if (auto result = PerformControlTransfer(
                    USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE,
                    USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_DEVICE, 0), 0,
                    &ud_descr_device, &len, false);
                result.IsFailure())
                return result;
        }

        {
            /* Obtain the language ID of this device */
            struct USB_DESCR_STRING s;
            size_t len = 4 /* just the first language */;
            if (auto result = PerformControlTransfer(
                    USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE,
                    USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_STRING, 0), 0, &s,
                    &len, false);
                result.IsFailure())
                return result;

            /* Retrieved string language code */
            uint16_t langid = s.u.str_langid[0];

            /* Time to fetch strings; this must be done in two steps: length and content */
            len = 4 /* length only */;
            if (auto result = PerformControlTransfer(
                    USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE,
                    USB_CONTROL_TYPE_STANDARD,
                    USB_REQUEST_MAKE(USB_DESCR_TYPE_STRING, ud_descr_device.dev_productidx), langid,
                    &s, &len, false);
                result.IsFailure())
                return result;

            /* Retrieved string length */

            /* Fetch the entire string this time */
            char tmp[1024]; /* XXX */
            auto s_full = reinterpret_cast<struct USB_DESCR_STRING*>(tmp);
            len = s.str_length;
            KASSERT(len < sizeof(tmp), "very large string descriptor %u", s.str_length);
            if (auto result = PerformControlTransfer(
                    USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE,
                    USB_CONTROL_TYPE_STANDARD,
                    USB_REQUEST_MAKE(USB_DESCR_TYPE_STRING, ud_descr_device.dev_productidx), langid,
                    s_full, &len, false);
                result.IsFailure())
                return result;

            kprintf("product <");
            for (int i = 0; i < s_full->str_length / 2; i++) {
                kprintf("%c", s_full->u.str_string[i] & 0xff);
            }
            kprintf(">\n");
        }

        /*
         * Grab the first few bytes of configuration descriptor. Note that we
         * have no idea how long the configuration exactly is, so we must
         * do this in two steps.
         */
        {
            struct USB_DESCR_CONFIG c;
            size_t len = sizeof(c);
            if (auto result = PerformControlTransfer(
                    USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE,
                    USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_CONFIG, 0), 0, &c,
                    &len, false);
                result.IsFailure())
                return result;

            /* Fetch the full descriptor */
            char tmp[1024]; /* XXX */
            auto c_full = reinterpret_cast<struct USB_DESCR_CONFIG*>(tmp);
            len = c.cfg_totallen;
            KASSERT(len < sizeof(tmp), "very large configuration descriptor %u", c.cfg_totallen);
            if (auto result = PerformControlTransfer(
                    USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE,
                    USB_CONTROL_TYPE_STANDARD, USB_REQUEST_MAKE(USB_DESCR_TYPE_CONFIG, 0), 0,
                    c_full, &len, false);
                result.IsFailure())
                return result;

            /* Handle the configuration */
            if (auto result =
                    ParseConfiguration(ud_interface, ud_num_interfaces, c_full, c.cfg_totallen);
                result.IsFailure())
                return result;

            /* For now, we'll just activate the very first configuration */
            if (auto result = PerformControlTransfer(
                    USB_CONTROL_REQUEST_SET_CONFIGURATION, USB_CONTROL_RECIPIENT_DEVICE,
                    USB_CONTROL_TYPE_STANDARD, c_full->cfg_identifier, 0, NULL, NULL, true);
                result.IsFailure())
                return result;

            /* Configuration activated */
            ud_cur_interface = 0;
        }

        /* Now, we'll have to hook up some driver... */
        ResourceSet resourceSet;
        resourceSet.AddResource(
            Resource(Resource::RT_USB_Device, reinterpret_cast<Resource::Base>(this), 0));
        ud_device = device_manager::AttachChild(ud_bus, resourceSet);
        KASSERT(ud_device != nullptr, "unable to find USB device to attach?");

        return Result::Success();
    }

    /* Called with bus lock held */
    Result USBDevice::Detach()
    {
        ud_bus.AssertLocked();

        // Stop any pending pipes - this should cancel any pending transfers, which ensures Detach()
        // can run without any callbacks in between
        Lock();
        for (auto& pipe : ud_pipes) {
            pipe.p_xfer.Cancel_Locked();
        }
        Unlock();

        // Ask the device to clean up after itself
        if (ud_device != nullptr) {
            if (auto result = device_manager::Detach(*ud_device); result.IsFailure())
                return result;
            ud_device = nullptr;
        }

        Lock();
        KASSERT(ud_pipes.empty(), "device detach with active pipes");
        KASSERT(ud_transfers.empty(), "device detach with active transfers");

        /* Remove the device from the bus - note that we hold the bus lock */
        ud_bus.bus_devices.remove(*this);

        // All set; the device is eligable for destruction now
        Unlock();
        return Result::Success();
    }

    namespace
    {
        void CallbackWrapper(Transfer& xfer)
        {
            auto pipe = static_cast<Pipe*>(xfer.t_callback_data);
            pipe->p_callback.OnPipeCallback(*pipe);
        }

    } // unnamed namespace

    Result USBDevice::AllocatePipe(
        int num, int type, int dir, size_t maxlen, IPipeCallback& callback, Pipe*& pipe)
    {
        KASSERT(dir == EP_DIR_IN || dir == EP_DIR_OUT, "invalid direction %u", dir);
        KASSERT(
            type == TRANSFER_TYPE_CONTROL || type == TRANSFER_TYPE_INTERRUPT ||
                type == TRANSFER_TYPE_BULK || type == TRANSFER_TYPE_ISOCHRONOUS,
            "invalid type %u", type);

        Interface& uif = ud_interface[ud_cur_interface];
        if (num < 0 || num >= uif.if_num_endpoints)
            return Result::Failure(EINVAL);

        Endpoint& ep = uif.if_endpoint[num];
        if (ep.ep_type != type || ep.ep_dir != dir)
            return Result::Failure(EINVAL);

        if (maxlen == 0)
            maxlen = ep.ep_maxpacketsize;

        Transfer* xfer = AllocateTransfer(
            *this, ep.ep_type,
            ((ep.ep_dir == EP_DIR_IN) ? TRANSFER_FLAG_READ : TRANSFER_FLAG_WRITE) |
                TRANSFER_FLAG_DATA,
            ep.ep_address, maxlen);
        auto p = new Pipe(*this, ep, *xfer, callback);

        xfer->t_length = ep.ep_maxpacketsize;
        xfer->t_callback = CallbackWrapper;
        xfer->t_callback_data = p;

        /* Hook up the pipe to the device */
        Lock();
        ud_pipes.push_back(*p);
        Unlock();
        pipe = p;
        return Result::Success();
    }

    void USBDevice::FreePipe(Pipe& pipe)
    {
        Lock();
        FreePipe_Locked(pipe);
        Unlock();
    }

    void USBDevice::FreePipe_Locked(Pipe& pipe)
    {
        /* Free the transfer (this cancels it as well) */
        FreeTransfer_Locked(pipe.p_xfer);

        /* Unregister the pipe - this is where we need the lock for */
        ud_pipes.remove(pipe);
        delete &pipe;
    }

    Result Pipe::Start() { return p_xfer.Schedule(); }

    Result Pipe::Stop() { return p_xfer.Cancel(); }

    Result USBDevice::PerformControlTransfer(
        int req, int recipient, int type, int value, int index, void* buf, size_t* len, bool write)
    {
        int flags = 0;
        if (buf != nullptr)
            flags |= TRANSFER_FLAG_DATA;
        if (write)
            flags |= TRANSFER_FLAG_WRITE;
        else
            flags |= TRANSFER_FLAG_READ;
        auto xfer =
            AllocateTransfer(*this, TRANSFER_TYPE_CONTROL, flags, 0, (len != NULL) ? *len : 0);
        xfer->t_control_req.req_type = TO_REG32(
            (write ? 0 : USB_CONTROL_REQ_DEV2HOST) | USB_CONTROL_REQ_RECIPIENT(recipient) |
            USB_CONTROL_REQ_TYPE(type));
        xfer->t_control_req.req_request = TO_REG32(req);
        xfer->t_control_req.req_value = TO_REG32(value);
        xfer->t_control_req.req_index = index;
        xfer->t_control_req.req_length = (len != NULL) ? *len : 0;
        xfer->t_length = (len != NULL) ? *len : 0;

        /* Now schedule the transfer and until it's completed XXX Timeout */
        xfer->Schedule();
        xfer->t_semaphore.WaitAndDrain();
        if (xfer->t_flags & TRANSFER_FLAG_ERROR) {
            FreeTransfer(*xfer);
            return Result::Failure(EIO);
        }

        if (buf != nullptr && len != nullptr) {
            /* XXX Ensure we don't return more than the user wants to know */
            if (*len > xfer->t_result_length)
                *len = xfer->t_result_length;
            memcpy(buf, xfer->t_data, *len);
        }

        FreeTransfer(*xfer);
        return Result::Success();
    }

} // namespace usb
