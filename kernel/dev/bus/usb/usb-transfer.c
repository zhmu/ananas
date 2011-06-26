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
#include <ananas/waitqueue.h>
#include <ananas/mm.h>

TRACE_SETUP;

errorcode_t
usb_control_xfer(struct USB_DEVICE* usb_dev, int req, int type, int value, int index, void* buf, size_t* len, int write)
{
	/* Make the USB transfer itself */
	int flags = 0;
	if (buf != NULL)
		flags |= TRANSFER_FLAG_DATA;
	if (write)
		flags |= TRANSFER_FLAG_WRITE;
	else
		flags |= TRANSFER_FLAG_READ;
	struct USB_TRANSFER* xfer = usb_alloc_transfer(usb_dev, TRANSFER_TYPE_CONTROL, flags, 0);
	xfer->xfer_control_req.req_type = TO_REG32((write ? USB_CONTROL_RECIPIENT_DEVICE : USB_CONTROL_REQ_DEV2HOST) | USB_CONTROL_RECIPIENT_DEVICE | USB_CONTROL_REQ_TYPE(type));
	xfer->xfer_control_req.req_request = TO_REG32(req);
	xfer->xfer_control_req.req_value = TO_REG32(value);
	xfer->xfer_control_req.req_index = index;
	xfer->xfer_control_req.req_length = (len != NULL) ? *len : 0;
	xfer->xfer_length = (len != NULL) ? *len : 0;

	/* Now schedule the transfer and until it's completed XXX Timeout */
	struct WAITER* w = waitqueue_add(&xfer->xfer_waitqueue);
	usb_schedule_transfer(xfer);
	waitqueue_wait(w);
	if (xfer->xfer_flags & TRANSFER_FLAG_ERROR)
		return ANANAS_ERROR(IO); /* XXX free xfer */

	if (buf != NULL && len != NULL) {
		/* XXX Ensure we don't return more than the user wants to know */
		if (*len > xfer->xfer_result_length)
			*len = xfer->xfer_result_length;
		memcpy(buf, xfer->xfer_data, *len);
	}
	/* XXX free xfer */
	return ANANAS_ERROR_OK;
}

/* vim:set ts=2 sw=2: */
