#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/bus/usb/core.h>
#include <ananas/bus/usb/config.h>
#include <ananas/bus/usb/descriptor.h>
#include <ananas/dqueue.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/trace.h>
#include <ananas/mm.h>

TRACE_SETUP;

static int
usb_find_type(void** data, int* left, int type, void** out)
{
	while (*left > 0) {
		struct USB_DESCR_GENERIC* g = *data;

		/* Already update the data pointer */
		*data += g->gen_length;
		*left -= g->gen_length;
		if (g->gen_type == type) {
			/* Found it */
			*out = g;
			return 1;
		}
	}
	return 0;
}

errorcode_t
usb_parse_configuration(struct USB_DEVICE* usb_dev, void* data, int datalen)
{
	struct DEVICE* dev = usb_dev->usb_device;
	struct USB_DESCR_CONFIG* cfg = data;
	if (cfg->cfg_type != USB_DESCR_TYPE_CONFIG)
		return ANANAS_ERROR(BAD_TYPE);

	/* Walk over all interfaces */
	void* ptr = data + cfg->cfg_length;
	int left = datalen - cfg->cfg_length;
	for (int ifacenum = 0; ifacenum < cfg->cfg_numinterfaces; ifacenum++) {
		/* Find the interface */
		struct USB_DESCR_INTERFACE* iface;
		if (!usb_find_type(&ptr, &left, USB_DESCR_TYPE_INTERFACE, (void**)&iface))
			return ANANAS_ERROR(NO_RESOURCE);

		TRACE_DEV(USB, INFO, dev,
		 "interface%u: class=%u, subclass=%u, protocol=%u, %u endpoint(s)",
		 iface->if_number, iface->if_class, iface->if_subclass, iface->if_protocol,
		 iface->if_numendpoints);

		/* Create the USB interface */
		struct USB_INTERFACE* usb_iface = &usb_dev->usb_interface[usb_dev->usb_num_interfaces++];
		usb_iface->if_class = iface->if_class;
		usb_iface->if_subclass = iface->if_subclass;
		usb_iface->if_protocol = iface->if_protocol;

		/* Handle all endpoints */
		for (int epnum = 0; epnum < iface->if_numendpoints; epnum++) {
			struct USB_DESCR_ENDPOINT* ep;
			if (!usb_find_type(&ptr, &left, USB_DESCR_TYPE_ENDPOINT, (void**)&ep))
				return ANANAS_ERROR(NO_RESOURCE);

			TRACE_DEV(USB, INFO, dev,
			 "endpoint %u: addr=%s:%u, attr=%u, maxpacketsz=%u",
			 epnum, (ep->ep_addr & USB_EP_ADDR_IN) ? "in" : "out",
			 USB_EP_ADDR(ep->ep_addr), USB_PE_ATTR_TYPE(ep->ep_attr),
			 ep->ep_maxpacketsz);

			/* Resolve the endpoint type to our own */
			int type = -1;
			switch(USB_PE_ATTR_TYPE(ep->ep_attr)) {
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
			usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_device = usb_dev;
			usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_address = USB_EP_ADDR(ep->ep_addr);
			usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_type = type;
			usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_dir = (ep->ep_addr & USB_EP_ADDR_IN) ? EP_DIR_IN : EP_DIR_OUT;
			usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_maxpacketsize = ep->ep_maxpacketsz;
			usb_iface->if_endpoint[usb_iface->if_num_endpoints].ep_interval = ep->ep_interval;
			usb_iface->if_num_endpoints++;
		}
	}
	
	return ANANAS_ERROR_NONE;
}

/* vim:set ts=2 sw=2: */
