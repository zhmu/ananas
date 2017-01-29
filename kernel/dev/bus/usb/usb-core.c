#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/bus/usb/core.h>
#include <ananas/lib.h>
#include <ananas/init.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/lock.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include "usb-bus.h"
#include "usb-device.h"
#include "usb-transfer.h"

static errorcode_t
usb_init()
{
	usbtransfer_init();
	usbbus_init();
	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(usb_init, SUBSYSTEM_DEVICE, ORDER_FIRST);

/* vim:set ts=2 sw=2: */
