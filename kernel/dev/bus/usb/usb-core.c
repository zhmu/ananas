#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/bus/usb/core.h>
#include <ananas/dqueue.h>
#include <ananas/lib.h>
#include <ananas/init.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/lock.h>
#include <ananas/mm.h>
#include <machine/param.h> /* for PAGE_SIZE */

static thread_t usb_workerthread;
static semaphore_t usb_sem;
static struct USB_TRANSFER_QUEUE usb_xfer_pendingqueue;
static spinlock_t spl_usb_xfer_pendingqueue = SPINLOCK_DEFAULT_INIT;

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
	DQUEUE_INIT(&usb_dev->usb_pipes);
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
	sem_init(&usb_xfer->xfer_semaphore, 0);
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
	 * process the transfer; if the transfer doesn't have a callback function,
	 * assume we'll just have to signal its semaphore.
	 */
	if (xfer->xfer_callback != NULL) {
		spinlock_lock(&spl_usb_xfer_pendingqueue);
		DQUEUE_ADD_TAIL(&usb_xfer_pendingqueue, xfer);
		spinlock_unlock(&spl_usb_xfer_pendingqueue);

		sem_signal(&usb_sem);
	} else {
		sem_signal(&xfer->xfer_semaphore);
	}
}

static void
usb_thread(void* arg)
{
	while(1) {
		/* Wait until there's something to report */
		sem_wait(&usb_sem);

		/* Fetch an entry from the queue */
		while (1) {
			spinlock_lock(&spl_usb_xfer_pendingqueue);
			if(DQUEUE_EMPTY(&usb_xfer_pendingqueue)) {
				spinlock_unlock(&spl_usb_xfer_pendingqueue);
				break;
			}
			struct USB_TRANSFER* xfer = DQUEUE_HEAD(&usb_xfer_pendingqueue);
			DQUEUE_POP_HEAD(&usb_xfer_pendingqueue);
			spinlock_unlock(&spl_usb_xfer_pendingqueue);
		
			/* And handle it */
			if (xfer->xfer_callback)
				xfer->xfer_callback(xfer);

			/* We needn't free interrupt transfers - they are cancelled by the requestor as needed */
			if (xfer->xfer_type == TRANSFER_TYPE_INTERRUPT) {
				/* Re-schedule the interrupt transfer */
				if (usb_schedule_transfer(xfer) != ANANAS_ERROR_OK) {
					device_printf(xfer->xfer_device->usb_hub, "XXX unable to reschedule, cancelling interrupt transfer!");
					usb_free_transfer(xfer);
				}
			} else {
				usb_free_transfer(xfer);
			}
		}
	}
}

static errorcode_t
usb_init()
{
	DQUEUE_INIT(&usb_xfer_pendingqueue);
	sem_init(&usb_sem, 0);

	/* Create a kernel thread to handle USB completed messages */
	kthread_init(&usb_workerthread, &usb_thread, NULL);
	thread_set_args(&usb_workerthread, "[usb]\0\0", PAGE_SIZE);
	thread_resume(&usb_workerthread);

	usb_attach_init();
	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(usb_init, SUBSYSTEM_DEVICE, ORDER_FIRST);

/* vim:set ts=2 sw=2: */
