#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/bus/usb/core.h>
#include <ananas/dqueue.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE */

static struct THREAD usb_workerthread;
static struct USB_TRANSFER_QUEUE usb_xfer_pendingqueue;
static spinlock_t spl_usb_xfer_pendingqueue;

struct DRIVER drv_usb = {
	.name = "usbdev"
};

struct USB_DEVICE*
usb_alloc_device(device_t root, device_t hub, void* hcd_privdata)
{
	struct USB_DEVICE* usb_dev = kmalloc(sizeof *usb_dev);
	memset(usb_dev, 0, sizeof *usb_dev);
	usb_dev->usb_device = device_alloc(root, &drv_usb);
	usb_dev->usb_hcd_privdata = hcd_privdata;
	usb_dev->usb_hub = hub;
	usb_dev->usb_device->privdata = usb_dev;
	usb_dev->usb_max_packet_sz0 = USB_DEVICE_DEFAULT_MAX_PACKET_SZ0;
	usb_dev->usb_address = 0;
	usb_dev->usb_devstatus = STATUS_DEFAULT;
	usb_dev->usb_attachstep = -1;
	usb_dev->usb_num_interfaces = 0;
	usb_dev->usb_cur_interface = -1;
	usb_dev->usb_langid = 0;
	return usb_dev;
}

void
usb_free_device(struct USB_DEVICE* usb_dev)
{
	device_free(usb_dev->usb_device);
	kfree(usb_dev);
}

struct USB_TRANSFER*
usb_alloc_transfer(struct USB_DEVICE* dev, int type, int flags, int endpt)
{
	struct USB_TRANSFER* usb_xfer = kmalloc(sizeof *usb_xfer);
	memset(usb_xfer, 0, sizeof *usb_xfer);
	usb_xfer->xfer_device = dev;
	usb_xfer->xfer_type = type;
	usb_xfer->xfer_flags = flags;
	usb_xfer->xfer_address = dev->usb_address;
	usb_xfer->xfer_endpoint = endpt;
	return usb_xfer;
}

errorcode_t
usb_schedule_transfer(struct USB_TRANSFER* xfer)
{
	struct DEVICE* dev = xfer->xfer_device->usb_device->parent;
	KASSERT(dev->driver->drv_usb_xfer != NULL, "transferring without usb transfer");
	return dev->driver->drv_usb_xfer(dev, xfer);
}

void
usb_free_transfer(struct USB_TRANSFER* xfer)
{
	/* XXX We hope it's not active */
//	kprintf("usb_free_transfer(): xfer=%p\n", xfer);
	kfree(xfer);
}

int
usb_get_next_address(struct USB_DEVICE* usb_dev)
{
	static int usb_next_device_addr = 1; /* XXX this is gross */
	return usb_next_device_addr++;
}

void
usb_completed_transfer(struct USB_TRANSFER* xfer)
{
	/*
	 * This is generally called from interrupt context, so schedule a worker to
	 * process the transfer.
	 */
	spinlock_lock(&spl_usb_xfer_pendingqueue);
	DQUEUE_ADD_TAIL(&usb_xfer_pendingqueue, xfer);
	spinlock_unlock(&spl_usb_xfer_pendingqueue);

	thread_resume(&usb_workerthread);
}

static void
usb_thread(void* arg)
{
	thread_t thread = PCPU_GET(curthread);
	while(1) {
		/* Fetch an entry from the queue */
		spinlock_lock(&spl_usb_xfer_pendingqueue);
		KASSERT(!DQUEUE_EMPTY(&usb_xfer_pendingqueue), "queue empty?");
		struct USB_TRANSFER* xfer = DQUEUE_HEAD(&usb_xfer_pendingqueue);
		DQUEUE_POP_HEAD(&usb_xfer_pendingqueue);
		spinlock_unlock(&spl_usb_xfer_pendingqueue);
		
		/* And handle it */
		if (xfer->xfer_callback) {
			xfer->xfer_callback(xfer);
		}
		usb_free_transfer(xfer);

		thread_suspend(thread);
		reschedule();
	}
}

void
usb_init()
{
	DQUEUE_INIT(&usb_xfer_pendingqueue);
	spinlock_init(&spl_usb_xfer_pendingqueue);

	/* Create a kernel thread to handle USB completed messages */
  thread_init(&usb_workerthread, NULL);
  thread_set_args(&usb_workerthread, "[usb]\0\0", PAGE_SIZE);
	md_thread_setkthread(&usb_workerthread, usb_thread, NULL);
}

/* vim:set ts=2 sw=2: */
