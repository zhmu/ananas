#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/bus/usb/config.h>
#include <ananas/bus/usb/core.h>
#include <ananas/dqueue.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/mm.h>

TRACE_SETUP;

static errorcode_t
usbkbd_probe(device_t dev)
{
	struct USB_DEVICE* usb_dev = dev->parent->privdata;

	/* XXX This is crude */
	struct USB_INTERFACE* iface = &usb_dev->usb_interface[usb_dev->usb_cur_interface];
	if (iface->if_class != USB_IF_CLASS_HID || iface->if_protocol != 1 /* keyboard */)
		return ANANAS_ERROR(NO_DEVICE);

	return ANANAS_ERROR_OK;
}

static void
usbkbd_callback(struct USB_TRANSFER* xfer)
{
	kprintf("usbkbd_callback! -> [");

	if (xfer->xfer_flags & TRANSFER_FLAG_ERROR) {
		kprintf("error, aborting]\n");
		return;
	}

	for (int i = 0; i < 8; i++) {
		kprintf("%x ", xfer->xfer_data[i]);
	}
	kprintf("]\n");

	struct USB_DEVICE* usb_dev = xfer->xfer_device;
	struct USB_ENDPOINT* ep = &usb_dev->usb_interface[usb_dev->usb_cur_interface].if_endpoint[0];

	struct USB_TRANSFER* new_xfer = usb_alloc_transfer(usb_dev, TRANSFER_TYPE_INTERRUPT, TRANSFER_FLAG_READ | TRANSFER_FLAG_DATA, ep->ep_address);
	new_xfer->xfer_length = ep->ep_maxpacketsize;
	new_xfer->xfer_callback = usbkbd_callback;
	usb_schedule_transfer(new_xfer);
}

static errorcode_t
usbkbd_attach(device_t dev)
{
	struct USB_DEVICE* usb_dev = dev->parent->privdata;

	/*
	 * OK; there's a keyboard here we want to attach. There must be an
	 * interrupt IN endpoint; this is where we get our data from.
	 */
	struct USB_ENDPOINT* ep = &usb_dev->usb_interface[usb_dev->usb_cur_interface].if_endpoint[0];
	if (ep->ep_type != TRANSFER_TYPE_INTERRUPT || ep->ep_dir != EP_DIR_IN) {
		device_printf(dev, "endpoint 0 not interrupt/in");
		return ANANAS_ERROR(NO_RESOURCE);
	}

	struct USB_TRANSFER* xfer = usb_alloc_transfer(usb_dev, TRANSFER_TYPE_INTERRUPT, TRANSFER_FLAG_READ | TRANSFER_FLAG_DATA, ep->ep_address);
	xfer->xfer_length = ep->ep_maxpacketsize;
	xfer->xfer_callback = usbkbd_callback;
	usb_schedule_transfer(xfer);
	
	return ANANAS_ERROR_OK;
}

struct DRIVER drv_usbkeyboard = {
	.name = "usbkeyboard",
	.drv_probe = usbkbd_probe,
	.drv_attach = usbkbd_attach
};

DRIVER_PROBE(usbkeyboard)
DRIVER_PROBE_BUS(usbdev)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
