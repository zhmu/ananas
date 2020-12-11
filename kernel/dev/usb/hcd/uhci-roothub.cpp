/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * UHCI root hub
 */
#include <ananas/types.h>
#include "kernel/dev/pci.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/schedule.h" // XXX
#include "kernel/thread.h"
#include "kernel/time.h"
#include "kernel-md/io.h"
#include "../core/descriptor.h"
#include "../core/usb-bus.h"
#include "../core/usb-core.h"
#include "../core/usb-device.h"
#include "../core/usb-transfer.h"
#include "uhci-reg.h"
#include "uhci-hcd.h"
#include "uhci-roothub.h"

namespace usb
{
    namespace uhci
    {
#if 0
#define DPRINTF kprintf
#else
#define DPRINTF(...)
#endif

        namespace
        {
            const struct USB_DESCR_DEVICE uhci_rh_device = {
                .dev_length = sizeof(struct USB_DESCR_DEVICE),
                .dev_type = 0,
                .dev_version = 0x101,
                .dev_class = USB_DESCR_CLASS_HUB,
                .dev_subclass = 0,
                .dev_protocol = 0,
                .dev_maxsize0 = 8,
                .dev_vendor = 0,
                .dev_product = 0,
                .dev_release = 0,
                .dev_manufactureridx = 2,
                .dev_productidx = 1,
                .dev_serialidx = 0,
                .dev_num_configs = 1,
            };

            const struct uhci_rh_string {
                uint8_t s_len, s_type;
                uint16_t s_string[13];
            } uhci_rh_strings[] = {
                /* supported languages */
                {.s_len = 4, .s_type = USB_DESCR_TYPE_STRING, .s_string = {1033}},
                /* Product ID */
                {.s_len = 28,
                 .s_type = USB_DESCR_TYPE_STRING,
                 .s_string = {'U', 'H', 'C', 'I', ' ', 'r', 'o', 'o', 't', ' ', 'h', 'u', 'b'}},
                /* Vendor ID */
                {.s_len = 14,
                 .s_type = USB_DESCR_TYPE_STRING,
                 .s_string = {'A', 'n', 'a', 'n', 'a', 's'}},
            };

            struct {
                struct USB_DESCR_CONFIG d_config;
                struct USB_DESCR_INTERFACE d_interface;
                struct USB_DESCR_ENDPOINT d_endpoint;
            } __attribute__((packed)) const uhci_rh_config = {
                /* Configuration */
                {
                    .cfg_length = sizeof(struct USB_DESCR_CONFIG),
                    .cfg_type = USB_DESCR_TYPE_CONFIG,
                    .cfg_totallen = sizeof(uhci_rh_config),
                    .cfg_numinterfaces = 1,
                    .cfg_identifier = 0,
                    .cfg_stringidx = 0,
                    .cfg_attrs = 0x40, /* self-powered */
                    .cfg_maxpower = 0,
                },
                /* Interface */
                {
                    .if_length = sizeof(struct USB_DESCR_INTERFACE),
                    .if_type = USB_DESCR_TYPE_INTERFACE,
                    .if_number = 1,
                    .if_altsetting = 0,
                    .if_numendpoints = 1,
                    .if_class = USB_IF_CLASS_HUB,
                    .if_subclass = 0,
                    .if_protocol = 1,
                    .if_interfaceidx = 0,
                },
                /* Endpoint */
                {
                    .ep_length = sizeof(struct USB_DESCR_ENDPOINT),
                    .ep_type = USB_DESCR_TYPE_ENDPOINT,
                    .ep_addr = USB_EP_ADDR_IN | USB_EP_ADDR(1),
                    .ep_attr = USB_PE_ATTR_TYPE_INTERRUPT,
                    .ep_maxpacketsz = 8,
                    .ep_interval = 255,
                }};

        } // unnamed namespace

        RootHub::RootHub(HCD_Resources& hcdResources, USBDevice& device)
            : rh_Resources(hcdResources), rh_Device(device)
        {
        }

        Result RootHub::ControlTransfer(Transfer& xfer)
        {
            struct USB_CONTROL_REQUEST* req = &xfer.t_control_req;
            Result result = RESULT_MAKE_FAILURE(EINVAL);

#define MASK(x) ((x) & (UHCI_PORTSC_SUSP | UHCI_PORTSC_RESET | UHCI_PORTSC_RD | UHCI_PORTSC_PORTEN))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

            switch (USB_REQUEST_MAKE(req->req_type, req->req_request)) {
                case USB_REQUEST_STANDARD_GET_DESCRIPTOR:
                    switch (req->req_value >> 8) {
                        case USB_DESCR_TYPE_DEVICE: {
                            int amount = MIN(uhci_rh_device.dev_length, req->req_length);
                            memcpy(xfer.t_data, &uhci_rh_device, amount);
                            xfer.t_result_length = amount;
                            result = Result::Success();
                            break;
                        }
                        case USB_DESCR_TYPE_STRING: {
                            int string_id = req->req_value & 0xff;
                            if (string_id >= 0 &&
                                string_id < sizeof(uhci_rh_strings) / sizeof(uhci_rh_strings[0])) {
                                int amount = MIN(uhci_rh_strings[string_id].s_len, req->req_length);
                                memcpy(xfer.t_data, &uhci_rh_strings[string_id], amount);
                                xfer.t_result_length = amount;
                                result = Result::Success();
                            }
                            break;
                        }
                        case USB_DESCR_TYPE_CONFIG: {
                            int amount = MIN(uhci_rh_config.d_config.cfg_totallen, req->req_length);
                            memcpy(xfer.t_data, &uhci_rh_config, amount);
                            xfer.t_result_length = amount;
                            result = Result::Success();
                            break;
                        }
                    }
                    break;
                case USB_REQUEST_STANDARD_SET_ADDRESS:
                    DPRINTF("set address: %d", req->req_value);
                    result = Result::Success();
                    break;
                case USB_REQUEST_STANDARD_SET_CONFIGURATION:
                    DPRINTF("set config: %d", req->req_value);
                    result = Result::Success();
                    break;
                case USB_REQUEST_CLEAR_HUB_FEATURE:
                    break;
                case USB_REQUEST_SET_HUB_FEATURE:
                    break;
                case USB_REQUEST_GET_BUS_STATE:
                    break;
                case USB_REQUEST_GET_HUB_DESCRIPTOR: {
                    /* First step is to construct our hub descriptor */
                    int port_len = (rh_numports + 7) / 8;
                    struct USB_DESCR_HUB hd;
                    memset(&hd, 0, sizeof(hd));
                    hd.hd_length = sizeof(hd) - (HUB_MAX_PORTS + 7) / 8 + port_len;
                    hd.hd_type = USB_DESCR_TYPE_HUB;
                    hd.hd_numports = rh_numports;
                    hd.hd_max_current = 0;

                    hd.hd_flags = USB_HD_FLAG_PS_INDIVIDUAL;
                    hd.hd_poweron2good = 50; /* 100ms */
                    /* All ports are removable; no need to do anything */
                    /* Copy the descriptor we just created */
                    int amount = MIN(hd.hd_length, req->req_length);
                    memcpy(xfer.t_data, &hd, amount);
                    xfer.t_result_length = amount;
                    result = Result::Success();
                    break;
                }
                case USB_REQUEST_GET_HUB_STATUS: {
                    if (req->req_value == 0 && req->req_index == 0 && req->req_length == 4) {
                        uint32_t hs = 0;
                        /* XXX over-current */
                        memcpy(xfer.t_data, &hs, sizeof(hs));
                        xfer.t_result_length = sizeof(hs);
                        result = Result::Success();
                    }
                    break;
                }
                case USB_REQUEST_GET_PORT_STATUS: {
                    if (req->req_value == 0 && req->req_index >= 1 &&
                        req->req_index <= rh_numports && req->req_length == 4) {
                        int reg = UHCI_REG_PORTSC1 + (req->req_index - 1) * 2;

                        struct USB_HUB_PORTSTATUS ps;
                        ps.ps_portstatus = USB_HUB_PS_PORT_POWER; /* always powered */
                        ps.ps_portchange = 0;

                        int portstat = rh_Resources.Read2(reg);
                        if (portstat & UHCI_PORTSC_SUSP)
                            ps.ps_portstatus |= USB_HUB_PS_PORT_SUSPEND;
                        if (portstat & UHCI_PORTSC_RESET)
                            ps.ps_portstatus |= USB_HUB_PS_PORT_RESET;
                        if (portstat & UHCI_PORTSC_LOWSPEED)
                            ps.ps_portstatus |= USB_HUB_PS_PORT_LOW_SPEED;
                        if (portstat & UHCI_PORTSC_PECHANGE)
                            ps.ps_portchange |= USB_HUB_PC_C_PORT_ENABLE;
                        if (portstat & UHCI_PORTSC_PORTEN)
                            ps.ps_portstatus |= USB_HUB_PS_PORT_ENABLE;
                        if (portstat & UHCI_PORTSC_CSCHANGE)
                            ps.ps_portchange |= USB_HUB_PC_C_PORT_CONNECTION;
                        if (portstat & UHCI_PORTSC_CONNSTAT)
                            ps.ps_portstatus |= USB_HUB_PS_PORT_CONNECTION;
                        if (rh_c_portreset) {
                            /* C_PORT_RESET is emulated manually */
                            ps.ps_portchange |= USB_HUB_PC_C_PORT_RESET;
                            rh_c_portreset = false;
                        }
                        /* XXX We don't do overcurrent */

                        memcpy(xfer.t_data, &ps, sizeof(ps));
                        xfer.t_result_length = sizeof(ps);
                        result = Result::Success();
                    }
                    break;
                }
                case USB_REQUEST_SET_PORT_FEATURE: {
                    unsigned int port = req->req_index;
                    if (port >= 1 && port <= rh_numports) {
                        int reg = UHCI_REG_PORTSC1 + (req->req_index - 1) * 2;
                        result = Result::Success();

                        switch (req->req_value) {
                            case HUB_FEATURE_PORT_RESET: {
                                DPRINTF("set port reset, port %d", req->req_index);

                                /* First step is to reset the port */
                                rh_Resources.Write2(
                                    reg, MASK(rh_Resources.Read2(reg)) | UHCI_PORTSC_RESET);
                                delay(200); /* port reset delay */
                                rh_Resources.Write2(
                                    reg, MASK(rh_Resources.Read2(reg)) & (~UHCI_PORTSC_RESET));
                                delay(100); /* device ready delay */

                                /* Now enable the port (required per 11.16.2.6.1.2) */
                                outw(port, MASK(rh_Resources.Read2(reg)) | UHCI_PORTSC_PORTEN);

                                /* Now see if the port becomes stable */
                                int n = 10;
                                for (/* nothing */; n > 0; n--) {
                                    delay(50); /* port reset delay */

                                    int stat = rh_Resources.Read2(reg);
                                    if ((stat & UHCI_PORTSC_CONNSTAT) == 0)
                                        break; /* Device removed during reset; give up */

                                    if (stat & (UHCI_PORTSC_PECHANGE | UHCI_PORTSC_CSCHANGE)) {
                                        /* Port enable / connect state changed; acknowledge them
                                         * both */
                                        rh_Resources.Write2(
                                            port,
                                            MASK(rh_Resources.Read2(port)) |
                                                (UHCI_PORTSC_PECHANGE | UHCI_PORTSC_CSCHANGE));
                                        continue;
                                    }

                                    if (stat & UHCI_PORTSC_PORTEN)
                                        break; /* port is enabled; we are done */

                                    /* Try harder to enable the port */
                                    rh_Resources.Write2(
                                        reg, MASK(rh_Resources.Read2(reg)) | UHCI_PORTSC_PORTEN);
                                }
                                if (n == 0) {
                                    kprintf("port %u not responding to reset", n);
                                    result = RESULT_MAKE_FAILURE(ENODEV);
                                } else {
                                    /* Used to emulate 'port reset changed'-bit */
                                    rh_c_portreset = true;
                                }
                                break;
                            }
                            case HUB_FEATURE_PORT_SUSPEND:
                                DPRINTF("set port suspend, port %d", req->req_index);
                                rh_Resources.Write2(
                                    reg, MASK(rh_Resources.Read2(reg)) | UHCI_PORTSC_SUSP);
                                result = Result::Success();
                                break;
                            case HUB_FEATURE_PORT_ENABLE:
                                /*
                                 * 11.16.2.6.1.2 states the hub's response to a
                                 * SetPortFeature(PORT_ENABLE) is not specified; we'll never issue
                                 * it and thus will reject the request (resetting ports must enable
                                 * them).
                                 */
                                break;
                            case HUB_FEATURE_PORT_POWER:
                                /* No-op, power is always enabled for us */
                                break;
                            default:
                                result = RESULT_MAKE_FAILURE(EINVAL);
                                break;
                        }
                    }
                    break;
                }
                case USB_REQUEST_CLEAR_PORT_FEATURE: {
                    unsigned int port = req->req_index;
                    if (port >= 1 && port <= rh_numports) {
                        int reg = UHCI_REG_PORTSC1 + (req->req_index - 1) * 2;
                        result = Result::Success();
                        switch (req->req_value) {
                            case HUB_FEATURE_PORT_ENABLE:
                                DPRINTF("HUB_FEATURE_PORT_ENABLE: port %d", req->req_index);
                                rh_Resources.Write2(
                                    reg, MASK(rh_Resources.Read2(reg)) & ~UHCI_PORTSC_PORTEN);
                                break;
                            case HUB_FEATURE_PORT_SUSPEND:
                                DPRINTF("HUB_FEATURE_PORT_SUSPEND: port %d", req->req_index);
                                rh_Resources.Write2(
                                    reg, MASK(rh_Resources.Read2(reg)) & ~UHCI_PORTSC_SUSP);
                                break;
                            case HUB_FEATURE_C_PORT_CONNECTION:
                                DPRINTF("HUB_FEATURE_C_PORT_CONNECTION: port %d", req->req_index);
                                rh_Resources.Write2(
                                    reg, MASK(rh_Resources.Read2(reg)) | UHCI_PORTSC_CSCHANGE);
                                break;
                            case HUB_FEATURE_C_PORT_RESET:
                                DPRINTF("HUB_FEATURE_C_PORT_RESET: port %d", req->req_index);
                                rh_c_portreset = false;
                                break;
                            case HUB_FEATURE_C_PORT_ENABLE:
                                DPRINTF("HUB_FEATURE_C_PORT_ENABLE: port %d", req->req_index);
                                rh_Resources.Write2(
                                    reg, MASK(rh_Resources.Read2(reg)) | UHCI_PORTSC_PECHANGE);
                                break;
                            default:
                                result = RESULT_MAKE_FAILURE(EINVAL);
                                break;
                        }
                    }
                    break;
                }
                default:
                    result = RESULT_MAKE_FAILURE(EINVAL);
                    break;
            }

#undef MIN
#undef MASK

            if (result.IsFailure()) {
                kprintf("oroothub: error %d\n", result.AsStatusCode());
                xfer.t_flags |= TRANSFER_FLAG_ERROR;
            }

            /* Immediately mark the transfer as completed */
            xfer.Complete_Locked();
            return result;
        }

        void RootHub::UpdateStatus()
        {
            /* Walk through every port, hungry for updates... */
            uint8_t hub_update = 0; /* max 7 ports, hub itself = 8 bits */
            int num_updates = 0;
            for (unsigned int n = 1; n <= rh_numports; n++) {
                int st = rh_Resources.Read2(UHCI_REG_PORTSC1 + (n - 1) * 2);
                if (st & (UHCI_PORTSC_PECHANGE | UHCI_PORTSC_CSCHANGE)) {
                    /* A changed event was triggered - need to report this */
                    hub_update |= 1 << (n % 8);
                    num_updates++;
                }
            }

            if (num_updates > 0) {
                int update_len = 1; /* always a single byte since we max the port count */

                /* Alter all entries in the transfer queue */
                rh_Device.Lock();
                for (auto& xfer : rh_Device.ud_transfers) {
                    if (xfer.t_type != TRANSFER_TYPE_INTERRUPT)
                        continue;
                    memcpy(&xfer.t_data, &hub_update, update_len);
                    xfer.t_result_length = update_len;
                    xfer.Complete_Locked();
                }
                rh_Device.Unlock();
            }
        }

        void RootHub::Thread()
        {
            while (1) {
                UpdateStatus();

                // Every second should be enough here...
                thread_sleep_ms(1000);
            }
        }

        Result RootHub::HandleTransfer(Transfer& xfer)
        {
            switch (xfer.t_type) {
                case TRANSFER_TYPE_CONTROL:
                    return ControlTransfer(xfer);
                case TRANSFER_TYPE_INTERRUPT:
                    /* Transfer has been added to the queue; no need to do anything else here */
                    return Result::Success();
            }
            panic("unsupported transfer type %d", xfer.t_type);
        }

        Result RootHub::Initialize()
        {
            /*
             * Note that there doesn't seem to be a way to ask the HC how much ports it
             * has; we try to take advantage of the fact that bit 7 is always set, so if
             * this is true and not all other bits are set as well, we assume there's a
             * port there...
             */
            rh_numports = 0;
            while (1) {
                int stat = rh_Resources.Read2(UHCI_REG_PORTSC1 + rh_numports * 2);
                if ((stat & UHCI_PORTSC_ALWAYS1) == 0 || (stat == 0xffff))
                    break;
                rh_numports++;
            }
            if (rh_numports > 7) {
                kprintf("detected excessive %d usb ports; aborting", rh_numports);
                return RESULT_MAKE_FAILURE(ENODEV);
            }

            /* Create a kernel thread to monitor status updates and process requests */
            if (auto result = kthread_alloc("uroothub", &ThreadWrapper, this, rh_pollthread);
                result.IsFailure())
                panic("cannot create uroothub thread");
            rh_pollthread->Resume();
            return Result::Success();
        }

    } // namespace uhci
} // namespace usb
