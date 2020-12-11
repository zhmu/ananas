/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * OHCI root hub
 */
#include <ananas/types.h>
#include "kernel/dev/pci.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/time.h"
#include "kernel/thread.h"
#include "kernel-md/io.h"
#include "../core/descriptor.h"
#include "../core/usb-core.h"
#include "../core/usb-bus.h"
#include "../core/usb-device.h"
#include "../core/usb-transfer.h"
#include "ohci-reg.h"
#include "ohci-hcd.h"
#include "ohci-roothub.h"

namespace usb
{
    namespace ohci
    {
        namespace
        {
#if 0
#define DPRINTF(s, ...) kprintf(s "\n", __VA_ARGS__)
#else
#define DPRINTF(...)
#endif

            const struct USB_DESCR_DEVICE ohci_rh_device = {
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

            const struct ohci_rh_string {
                uint8_t s_len, s_type;
                uint16_t s_string[13];
            } ohci_rh_strings[] = {
                /* supported languages */
                {.s_len = 4, .s_type = USB_DESCR_TYPE_STRING, .s_string = {1033}},
                /* Product ID */
                {.s_len = 28,
                 .s_type = USB_DESCR_TYPE_STRING,
                 .s_string = {'O', 'H', 'C', 'I', ' ', 'r', 'o', 'o', 't', ' ', 'h', 'u', 'b'}},
                /* Vendor ID */
                {.s_len = 14,
                 .s_type = USB_DESCR_TYPE_STRING,
                 .s_string = {'A', 'n', 'a', 'n', 'a', 's'}},
            };

            struct {
                struct USB_DESCR_CONFIG d_config;
                struct USB_DESCR_INTERFACE d_interface;
                struct USB_DESCR_ENDPOINT d_endpoint;
            } __attribute__((packed)) const ohci_rh_config = {
                /* Configuration */
                {
                    .cfg_length = sizeof(struct USB_DESCR_CONFIG),
                    .cfg_type = USB_DESCR_TYPE_CONFIG,
                    .cfg_totallen = sizeof(ohci_rh_config),
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

#define MIN(a, b) ((a) < (b) ? (a) : (b))

            switch (USB_REQUEST_MAKE(req->req_type, req->req_request)) {
                case USB_REQUEST_STANDARD_GET_DESCRIPTOR:
                    switch (req->req_value >> 8) {
                        case USB_DESCR_TYPE_DEVICE: {
                            int amount = MIN(ohci_rh_device.dev_length, req->req_length);
                            memcpy(xfer.t_data, &ohci_rh_device, amount);
                            xfer.t_result_length = amount;
                            result = Result::Success();
                            break;
                        }
                        case USB_DESCR_TYPE_STRING: {
                            int string_id = req->req_value & 0xff;
                            if (string_id >= 0 &&
                                string_id < sizeof(ohci_rh_strings) / sizeof(ohci_rh_strings[0])) {
                                int amount = MIN(ohci_rh_strings[string_id].s_len, req->req_length);
                                memcpy(xfer.t_data, &ohci_rh_strings[string_id], amount);
                                xfer.t_result_length = amount;
                                result = Result::Success();
                            }
                            break;
                        }
                        case USB_DESCR_TYPE_CONFIG: {
                            int amount = MIN(ohci_rh_config.d_config.cfg_totallen, req->req_length);
                            memcpy(xfer.t_data, &ohci_rh_config, amount);
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

                    uint32_t rhda = rh_Resources.Read4(OHCI_HCRHDESCRIPTORA);
                    hd.hd_flags = 0;
                    if ((rhda & (OHCI_RHDA_NPS | OHCI_RHDA_PSM)) == OHCI_RHDA_PSM)
                        hd.hd_flags |= USB_HD_FLAG_PS_INDIVIDUAL;
                    if (rhda & OHCI_RHDA_NOCP)
                        hd.hd_flags |= USB_HD_FLAG_OC_NONE;
                    else if (rhda & OHCI_RHDA_OCPM)
                        hd.hd_flags |= USB_HD_FLAG_OC_INDIVIDUAL;
                    else
                        hd.hd_flags |= USB_HD_FLAG_OC_GLOBAL;
                    hd.hd_poweron2good = OHCI_RHDA_POTPGT(rhda);

                    /* Fill out all removable bits */
                    uint32_t rhdb = rh_Resources.Read4(OHCI_HCRHDESCRIPTORB);
                    for (unsigned int n = 1; n < rh_numports; n++) {
                        if (rhdb & (1 << n))
                            hd.hd_removable[n / 8] |= 1 << (n & 7);
                    }

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
                        uint32_t st =
                            rh_Resources.Read4(OHCI_HCRHPORTSTATUSx + (req->req_index - 1) * 4);

                        struct USB_HUB_PORTSTATUS ps;
                        memset(&ps, 0, sizeof(ps));
                        ps.ps_portstatus = st & 0x31f;
                        ps.ps_portchange = (st >> 16) & 0x1f;
                        memcpy(xfer.t_data, &ps, sizeof(ps));
                        xfer.t_result_length = sizeof(ps);
                        result = Result::Success();
                    }
                    break;
                }
                case USB_REQUEST_SET_PORT_FEATURE: {
                    unsigned int port = req->req_index;
                    if (port >= 1 && port <= rh_numports) {
                        port = OHCI_HCRHPORTSTATUSx + (req->req_index - 1) * 4;
                        result = Result::Success();
                        switch (req->req_value) {
                            case HUB_FEATURE_PORT_RESET: {
                                /*
                                 * To reset a port, we'll enable the PRS bit. Once it's unset, the
                                 * hub will be resetting the device and trigger PRSC (which the hub
                                 * checks for)
                                 */
                                DPRINTF("set port reset, port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_PRS);
                                int n = 10;
                                while (n > 0 && (rh_Resources.Read4(port) & OHCI_RHPS_PRS) != 0) {
                                    delay(100);
                                    n--;
                                }
                                if (n == 0) {
                                    kprintf("oroothub: port %u not responding to reset", n);
                                    result = RESULT_MAKE_FAILURE(ENODEV);
                                }
                                break;
                            }
                            case HUB_FEATURE_PORT_SUSPEND:
                                DPRINTF("set port suspend, port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_PSS);
                                result = Result::Success();
                                break;
                            case HUB_FEATURE_PORT_POWER:
                                DPRINTF("set port power, port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_PPS);
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
                        port = OHCI_HCRHPORTSTATUSx + (req->req_index - 1) * 4;
                        result = Result::Success();
                        switch (req->req_value) {
                            case HUB_FEATURE_PORT_ENABLE:
                                DPRINTF("HUB_FEATURE_PORT_ENABLE: port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_CCS);
                                break;
                            case HUB_FEATURE_PORT_SUSPEND:
                                DPRINTF("HUB_FEATURE_PORT_SUSPEND: port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_POCI);
                                break;
                            case HUB_FEATURE_PORT_POWER:
                                DPRINTF("HUB_FEATURE_PORT_POWER: port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_LSDA);
                                break;
                            case HUB_FEATURE_C_PORT_CONNECTION:
                                DPRINTF("HUB_FEATURE_C_PORT_CONNECTION: port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_CSC);
                                /* Re-enable RHSC after clear is complete */
                                rh_Resources.Write4(OHCI_HCINTERRUPTENABLE, OHCI_IE_RHSC);
                                break;
                            case HUB_FEATURE_C_PORT_RESET:
                                DPRINTF("HUB_FEATURE_C_PORT_RESET: port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_PRSC);
                                /* Re-enable RHSC after reset is complete; otherwise it keeps
                                 * spamming */
                                rh_Resources.Write4(OHCI_HCINTERRUPTENABLE, OHCI_IE_RHSC);
                                break;
                            case HUB_FEATURE_C_PORT_ENABLE:
                                DPRINTF("HUB_FEATURE_C_PORT_ENABLE: port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_PESC);
                                break;
                            case HUB_FEATURE_C_PORT_SUSPEND:
                                DPRINTF("HUB_FEATURE_C_PORT_SUSPEND: port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_PSSC);
                                break;
                            case HUB_FEATURE_C_PORT_OVER_CURRENT:
                                DPRINTF("HUB_FEATURE_C_PORT_OVER_CURRENT: port %d", req->req_index);
                                rh_Resources.Write4(port, OHCI_RHPS_OCIC);
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

            if (result.IsFailure()) {
                kprintf("oroothub: error %d\n", result.AsStatusCode());
                xfer.t_flags |= TRANSFER_FLAG_ERROR;
            }

            /* Immediately mark the transfer as completed */
            xfer.Complete_Locked();
            return result;
        }

        void RootHub::ProcessInterruptTransfers()
        {
            /* Walk through every port, hungry for updates... */
            uint8_t hub_update[2] = {0, 0}; /* max 15 ports + hub status itself = 16 bits */
            int num_updates = 0;
            for (unsigned int n = 1; n <= rh_numports; n++) {
                int index = OHCI_HCRHPORTSTATUSx + (n - 1) * 4;
                uint32_t st = rh_Resources.Read4(index);
                if ((st >> 16) != 0) {
                    /* A changed event was triggered - need to report this */
                    hub_update[n / 8] |= 1 << (n % 8);
                    num_updates++;
                }
            }

            if (num_updates == 0)
                return;
            int update_len = (rh_numports + 1 /* hub */ + 7 /* round up */) / 8;

            /* Alter all entries in the transfer queue */
            for (auto& xfer : rh_Device.ud_transfers) {
                if (xfer.t_type != TRANSFER_TYPE_INTERRUPT)
                    continue;
                memcpy(&xfer.t_data, hub_update, update_len);
                xfer.t_result_length = update_len;
                xfer.Complete_Locked();
            }

            rh_pending_changes = false;
        }

        void RootHub::Thread()
        {
            while (1) {
                /* Wait until we get a roothub interrupt; that should signal something happened */
                rh_semaphore.WaitAndDrain();

                /*
                 * If we do not have anything in the interrupt queue, there is no need to
                 * bother checking as no one can handle yet - best to wait...
                 */
                rh_Device.Lock();
                ProcessInterruptTransfers();
                rh_Device.Unlock();
            }
        }

        Result RootHub::HandleTransfer(Transfer& xfer)
        {
            switch (xfer.t_type) {
                case TRANSFER_TYPE_CONTROL:
                    return ControlTransfer(xfer);
                case TRANSFER_TYPE_INTERRUPT:
                    /*
                     * The transfer will have been added to the list of pending interrupt
                     * transfers; however, we will only honor those if something changes (i.e.
                     * Thread() wakes up). If there are any pending changes, this transfer
                     * wouldn't see them.
                     *
                     * We avoid this by seeing if there are any pending changes. If there
                     * are, we force the transfer to be processed.
                     */
                    if (rh_pending_changes)
                        ProcessInterruptTransfers();
                    return Result::Success();
            }
            panic("unsupported transfer type %d", xfer.t_type);
        }

        void RootHub::OnIRQ()
        {
            rh_semaphore.Signal();

#if 0
		/*
		 * Disable the roothub irq, we'll re-enable it when the port has been reset
		 * to avoid excessive interrupts.
		 */
		rh_Resources.Write4(OHCI_HCINTERRUPTDISABLE, OHCI_ID_RHSC);
#endif
        }

        Result RootHub::Initialize()
        {
            uint32_t rhda = rh_Resources.Read4(OHCI_HCRHDESCRIPTORA);
            rh_numports = OHCI_RHDA_NDP(rhda);
            if (rh_numports < 1 || rh_numports > 15) {
                kprintf("oroothub: invalid number of %d port(s) present", rh_numports);
                return RESULT_MAKE_FAILURE(ENODEV);
            }

            /*
             * Launch the poll thread to handle the interrupt pipe requests; we can't do
             * that from oroothub_init() because the usbbus doesn't exist at that point
             * and we don't know the USB device either.
             */
            if (auto result = kthread_alloc("oroothub", &ThreadWrapper, this, rh_pollthread);
                result.IsFailure())
                panic("cannot create oroothub thread");
            rh_pollthread->Resume();
            return Result::Success();
        }

    } // namespace ohci
} // namespace usb
