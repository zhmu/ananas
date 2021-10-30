/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "usb-core.h"
#include "usb-device.h"
#include "usb-transfer.h"
#include "config.h"
#include "descriptor.h"

namespace usb
{
    namespace
    {
        template<typename T>
        bool LookupDescriptorByType(char*& data, size_t& left, int type, T*& out)
        {
            while (left > 0) {
                auto g = reinterpret_cast<struct USB_DESCR_GENERIC*>(data);

                /* Already update the data pointer */
                data += g->gen_length;
                left -= g->gen_length;
                if (g->gen_type == type) {
                    /* Found it */
                    out = reinterpret_cast<T*>(g);
                    return true;
                }
            }
            return false;
        }

    } // unnamed namespace

    Result ParseConfiguration(Interface* usb_if, int& usb_num_if, void* data, int datalen)
    {
        auto cfg = static_cast<struct USB_DESCR_CONFIG*>(data);
        if (cfg->cfg_type != USB_DESCR_TYPE_CONFIG)
            return Result::Failure(EINVAL);

        /* Walk over all interfaces */
        char* ptr = static_cast<char*>(data) + cfg->cfg_length;
        size_t left = datalen - cfg->cfg_length;
        for (int ifacenum = 0; ifacenum < cfg->cfg_numinterfaces; ifacenum++) {
            /* Find the interface */
            struct USB_DESCR_INTERFACE* iface;
            if (!LookupDescriptorByType(ptr, left, USB_DESCR_TYPE_INTERFACE, iface))
                return Result::Failure(EINVAL);

            /* Create the USB interface */
            Interface* usb_iface = &usb_if[usb_num_if++]; // XXX range check!
            usb_iface->if_class = iface->if_class;
            usb_iface->if_subclass = iface->if_subclass;
            usb_iface->if_protocol = iface->if_protocol;

            /* Handle all endpoints */
            for (int epnum = 0; epnum < iface->if_numendpoints; epnum++) {
                struct USB_DESCR_ENDPOINT* ep;
                if (!LookupDescriptorByType(ptr, left, USB_DESCR_TYPE_ENDPOINT, ep))
                    return Result::Failure(EINVAL);

                /* Resolve the endpoint type to our own */
                int type = -1;
                switch (USB_PE_ATTR_TYPE(ep->ep_attr)) {
                    case USB_PE_ATTR_TYPE_CONTROL:
                        type = TRANSFER_TYPE_CONTROL;
                        break;
                    case USB_PE_ATTR_TYPE_ISOCHRONOUS:
                        type = TRANSFER_TYPE_ISOCHRONOUS;
                        break;
                    case USB_PE_ATTR_TYPE_BULK:
                        type = TRANSFER_TYPE_BULK;
                        break;
                    case USB_PE_ATTR_TYPE_INTERRUPT:
                        type = TRANSFER_TYPE_INTERRUPT;
                        break;
                }

                /* Register the endpoint */
                usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_address =
                    USB_EP_ADDR(ep->ep_addr);
                usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_type = type;
                usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_dir =
                    (ep->ep_addr & USB_EP_ADDR_IN) ? EP_DIR_IN : EP_DIR_OUT;
                usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_maxpacketsize =
                    ep->ep_maxpacketsz;
                usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_interval = ep->ep_interval;
                usb_iface->if_num_endpoints++;
            }
        }

        return Result::Success();
    }

} // namespace usb
