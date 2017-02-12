#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/bus/usb/config.h>
#include <ananas/bus/usb/core.h>
#include <ananas/bus/usb/pipe.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include "usb-device.h"

TRACE_SETUP;

static errorcode_t
usbgeneric_probe(device_t dev)
{
	auto usb_dev = static_cast<struct USB_DEVICE*>(device_alloc_resource(dev, RESTYPE_USB_DEVICE, 0));
	if (usb_dev == NULL)
		return ANANAS_ERROR(NO_DEVICE);

	/* We always accept anything that is USB; usbdev_attach() will only attach us as a last resort */
	return ananas_success();
}

static errorcode_t
usbgeneric_attach(device_t dev)
{
	return ananas_success();
}

struct DRIVER drv_usbgeneric = {
	.name = "usbgeneric",
	.drv_probe = usbgeneric_probe,
	.drv_attach = usbgeneric_attach
};

/* vim:set ts=2 sw=2: */
