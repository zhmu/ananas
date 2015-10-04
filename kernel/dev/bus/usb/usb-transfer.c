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
#include <machine/param.h> /* XXX for PAGE_SIZE */
#include "usb-bus.h"
#include "usb-device.h"
#include "usb-transfer.h"

TRACE_SETUP;

static thread_t usbtransfer_thread;
static semaphore_t usbtransfer_sem;
static struct USB_TRANSFER_QUEUE usbtransfer_completedqueue;
static spinlock_t usbtransfer_lock = SPINLOCK_DEFAULT_INIT;

static struct USB_TRANSFER*
usb_make_control_xfer(struct USB_DEVICE* usb_dev, int req, int recipient, int type, int value, int index, void* buf, size_t* len, int write)
{
	/* Make the USB transfer itself */
	int flags = 0;
	if (buf != NULL)
		flags |= TRANSFER_FLAG_DATA;
	if (write)
		flags |= TRANSFER_FLAG_WRITE;
	else
		flags |= TRANSFER_FLAG_READ;
	struct USB_TRANSFER* xfer = usbtransfer_alloc(usb_dev, TRANSFER_TYPE_CONTROL, flags, 0);
	xfer->xfer_control_req.req_type = TO_REG32((write ? 0 : USB_CONTROL_REQ_DEV2HOST) | USB_CONTROL_REQ_RECIPIENT(recipient) | USB_CONTROL_REQ_TYPE(type));
	xfer->xfer_control_req.req_request = TO_REG32(req);
	xfer->xfer_control_req.req_value = TO_REG32(value);
	xfer->xfer_control_req.req_index = index;
	xfer->xfer_control_req.req_length = (len != NULL) ? *len : 0;
	xfer->xfer_length = (len != NULL) ? *len : 0;
	return xfer;
}

errorcode_t
usb_control_xfer(struct USB_DEVICE* usb_dev, int req, int recipient, int type, int value, int index, void* buf, size_t* len, int write)
{
	struct USB_TRANSFER* xfer = usb_make_control_xfer(usb_dev, req, recipient, type, value, index, buf, len, write);

	/* Now schedule the transfer and until it's completed XXX Timeout */
	usbtransfer_schedule(xfer);
	sem_wait_and_drain(&xfer->xfer_semaphore);
	if (xfer->xfer_flags & TRANSFER_FLAG_ERROR) {
		usbtransfer_free(xfer);
		return ANANAS_ERROR(IO);
	}

	if (buf != NULL && len != NULL) {
		/* XXX Ensure we don't return more than the user wants to know */
		if (*len > xfer->xfer_result_length)
			*len = xfer->xfer_result_length;
		memcpy(buf, xfer->xfer_data, *len);
	}

	usbtransfer_free(xfer);
	return ANANAS_ERROR_OK;
}

struct USB_TRANSFER*
usbtransfer_alloc(struct USB_DEVICE* dev, int type, int flags, int endpt)
{
	struct USB_TRANSFER* usb_xfer = kmalloc(sizeof *usb_xfer);
	//kprintf("usbtransfer_alloc: xfer=%x type %d\n", usb_xfer, type);
	memset(usb_xfer, 0, sizeof *usb_xfer);
	usb_xfer->xfer_device = dev;
	usb_xfer->xfer_type = type;
	usb_xfer->xfer_flags = flags;
	usb_xfer->xfer_address = dev->usb_address;
	usb_xfer->xfer_endpoint = endpt;
	sem_init(&usb_xfer->xfer_semaphore, 0);
	return usb_xfer;
}

static inline device_t
usbtransfer_get_hcd_device(struct USB_TRANSFER* xfer)
{
	/* All USB_DEVICE's are on usbbusX; the bus' parent is the HCD to use */
	KASSERT(xfer->xfer_device != NULL, "need a device");
	KASSERT(xfer->xfer_device->usb_bus != NULL, "need a bus");
	KASSERT(xfer->xfer_device->usb_bus->bus_dev != NULL, "need a bus device");
	return xfer->xfer_device->usb_bus->bus_dev->parent;
}

errorcode_t
usbtransfer_schedule(struct USB_TRANSFER* xfer)
{
	/* All USB_DEVICE's are on usbbusX; the bus' parent is the HCD to use */
	device_t hcd_dev = usbtransfer_get_hcd_device(xfer);
	KASSERT(hcd_dev->driver->drv_usb_schedule_xfer != NULL, "transferring without usb transfer");

	/* Schedule the transfer; we are responsible for locking here */
	mutex_lock(&xfer->xfer_device->usb_mutex);
	errorcode_t err = hcd_dev->driver->drv_usb_schedule_xfer(hcd_dev, xfer);
	mutex_unlock(&xfer->xfer_device->usb_mutex);
	return err;
}

void
usbtransfer_cancel_locked(struct USB_TRANSFER* xfer)
{
	mutex_assert(&xfer->xfer_device->usb_mutex, MTX_LOCKED);

	device_t hcd_dev = usbtransfer_get_hcd_device(xfer);
	if(hcd_dev->driver->drv_usb_cancel_xfer != NULL)
		hcd_dev->driver->drv_usb_cancel_xfer(hcd_dev, xfer);
}

void
usbtransfer_free_locked(struct USB_TRANSFER* xfer)
{
	mutex_assert(&xfer->xfer_device->usb_mutex, MTX_LOCKED);

	usbtransfer_cancel_locked(xfer);
	kfree(xfer);
}

void
usbtransfer_free(struct USB_TRANSFER* xfer)
{
	struct USB_DEVICE* usb_dev =xfer->xfer_device;

	//kprintf("usbtransfer_free: xfer=%x type %d\n", xfer, xfer->xfer_type);
	mutex_lock(&usb_dev->usb_mutex);
	usbtransfer_free_locked(xfer);
	mutex_unlock(&usb_dev->usb_mutex);
}

void
usbtransfer_complete(struct USB_TRANSFER* xfer)
{
	/*
	 * This is generally called from interrupt context, so schedule a worker to
	 * process the transfer; if the transfer doesn't have a callback function,
	 * assume we'll just have to signal its semaphore.
	 */
	if (xfer->xfer_callback != NULL) {
		spinlock_lock(&usbtransfer_lock);
		DQUEUE_ADD_TAIL_IP(&usbtransfer_completedqueue, completed, xfer);
		spinlock_unlock(&usbtransfer_lock);

		sem_signal(&usbtransfer_sem);
	} else {
		sem_signal(&xfer->xfer_semaphore);
	}
}

static void
transfer_thread(void* arg)
{
	while(1) {
		/* Wait until there's something to report */
		sem_wait(&usbtransfer_sem);

		/* Fetch an entry from the queue */
		while (1) {
			spinlock_lock(&usbtransfer_lock);
			if(DQUEUE_EMPTY(&usbtransfer_completedqueue)) {
				spinlock_unlock(&usbtransfer_lock);
				break;
			}
			struct USB_TRANSFER* xfer = DQUEUE_HEAD(&usbtransfer_completedqueue);
			DQUEUE_POP_HEAD_IP(&usbtransfer_completedqueue, completed);
			spinlock_unlock(&usbtransfer_lock);

			/* And handle it */
			if (xfer->xfer_callback)
				xfer->xfer_callback(xfer);

			/*
			 * At this point, xfer may be freed, resubmitted or simply left lingering for
			 * the caller - we can't touch it at this point.
			 */
		}
	}
}

void
usbtransfer_init()
{
	DQUEUE_INIT(&usbtransfer_completedqueue);
	sem_init(&usbtransfer_sem, 0);

	/* Create a kernel thread to handle USB completed messages */
	kthread_init(&usbtransfer_thread, &transfer_thread, NULL);
	thread_set_args(&usbtransfer_thread, "[usb-transfer]\0\0", PAGE_SIZE);
	thread_resume(&usbtransfer_thread);
}

/* vim:set ts=2 sw=2: */
