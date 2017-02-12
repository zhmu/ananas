/*
 * The usbbus is the root of all USB devices; it is connected directly to the
 * HCD driver device, and takes care of things like device attachment, power
 * and bandwidth regulation and the like.
 *
 * Directly attached to the usbbus are the actual USB devices; the USB_DEVICE
 * structure is obtained as a resource and takes care of the USB details.
 *
 * For example:
 *
 * ohci0 <--- usbbus0
 *               |
 *               +-----< usbhub0
 *               |
 *               +-----< usb-keyboard0
 */
#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/thread.h>
#include <machine/param.h> /* XXX for PAGE_SIZE */
#include "usb-bus.h"
#include "usb-device.h"

LIST_DEFINE(USB_BUSSES, struct USB_BUS);

static thread_t usbbus_thread;
static semaphore_t usbbus_semaphore;
static struct USB_DEVICES usbbus_pendingqueue;
static spinlock_t usbbus_spl_pendingqueue = SPINLOCK_DEFAULT_INIT; /* protects usbbus_pendingqueue */
static struct USB_BUSSES usbbus_busses;
static mutex_t usbbus_mutex; /* protects usbbus_busses */

void
usbbus_schedule_attach(struct USB_DEVICE* dev)
{
	/* Add the device to our queue */
	spinlock_lock(&usbbus_spl_pendingqueue);
	LIST_APPEND(&usbbus_pendingqueue, dev);
	spinlock_unlock(&usbbus_spl_pendingqueue);

	/* Wake up our thread */
	sem_signal(&usbbus_semaphore);
}

static errorcode_t
usbbus_attach(device_t dev)
{
	device_t hcd_dev = dev->parent; /* usbbus is attached to the HCD */

	/* Create our bus itself */
	auto bus = new(dev) USB_BUS;
	memset(bus, 0, sizeof *bus);
	bus->bus_dev = dev;
	bus->bus_hcd = hcd_dev;
	mutex_init(&bus->bus_mutex, "usbbus");
	LIST_INIT(&bus->bus_devices);
	bus->bus_flags = USB_BUS_FLAG_NEEDS_EXPLORE;
	dev->privdata = bus;

	/*
	 * Create the root hub device; it will handle all our children - the HCD may
	 * need to know about this as the root hub may be polling...
	  */
	struct USB_DEVICE* roothub_dev = usb_alloc_device(bus, NULL, 0, USB_DEVICE_FLAG_ROOT_HUB);
	if (hcd_dev->driver->drv_usb_set_roothub != NULL)
		hcd_dev->driver->drv_usb_set_roothub(bus->bus_hcd, roothub_dev);
	usbbus_schedule_attach(roothub_dev);

	/* Register ourselves within the big bus list */
	mutex_lock(&usbbus_mutex);
	LIST_APPEND(&usbbus_busses, bus);
	mutex_unlock(&usbbus_mutex);
	return ananas_success();
}

int
usbbus_alloc_address(struct USB_BUS* bus)
{
	/* XXX crude */
	static int cur_addr = 1;
	return cur_addr++;
}

void
usbbus_schedule_explore(struct USB_BUS* bus)
{
	mutex_lock(&bus->bus_mutex);
	bus->bus_flags |= USB_BUS_FLAG_NEEDS_EXPLORE;
	mutex_unlock(&bus->bus_mutex);

	sem_signal(&usbbus_semaphore);
}

/* Must be called with lock held! */
static void
usb_bus_explore(struct USB_BUS* bus)
{
	mutex_assert(&bus->bus_mutex, MTX_LOCKED);

	LIST_FOREACH(&bus->bus_devices, usb_dev, struct USB_DEVICE) {
		device_t dev = usb_dev->usb_device;
		if (dev->driver != NULL && dev->driver->drv_usb_explore != NULL) {
			dev->driver->drv_usb_explore(usb_dev);
		}
	}
}

/* Must be called with lock held! */
errorcode_t
usb_bus_detach_hub(struct USB_BUS* bus, struct USB_HUB* hub)
{
	mutex_assert(&bus->bus_mutex, MTX_LOCKED);

	LIST_FOREACH_SAFE(&bus->bus_devices, usb_dev, struct USB_DEVICE) {
		if (usb_dev->usb_hub != hub)
			continue;

		errorcode_t err = usbdev_detach(usb_dev);
		ANANAS_ERROR_RETURN(err);
	}

	return ananas_success();
}

static void
usb_bus_thread(void* unused)
{
	/* Note that this thread is used for _all_ USB busses */
	while(1) {
		/* Wait until we have to wake up... */
		sem_wait(&usbbus_semaphore);

		while(1) {
			/*
			 * See if any USB busses need to be explored; we do this first because
			 * exploring may trigger new devices to attach or old ones to remove.
			 */
			mutex_lock(&usbbus_mutex);
			LIST_FOREACH(&usbbus_busses, bus, struct USB_BUS) {
				mutex_lock(&bus->bus_mutex);
				if (bus->bus_flags & USB_BUS_FLAG_NEEDS_EXPLORE)
					usb_bus_explore(bus);
				mutex_unlock(&bus->bus_mutex);
			}
			mutex_unlock(&usbbus_mutex);

			/* Handle attaching devices */
			spinlock_lock(&usbbus_spl_pendingqueue);
			if (LIST_EMPTY(&usbbus_pendingqueue)) {
				spinlock_unlock(&usbbus_spl_pendingqueue);
				break;
			}
			struct USB_DEVICE* usb_dev = LIST_HEAD(&usbbus_pendingqueue);
			LIST_POP_HEAD(&usbbus_pendingqueue);
			spinlock_unlock(&usbbus_spl_pendingqueue);

			/*
			 * Note that attaching will block until completed, which is intentional
			 * as it ensures we will never attach more than one device in the system
			 * at any given time.
			 */
			errorcode_t err = usbdev_attach(usb_dev);
			KASSERT(ananas_is_success(err), "cannot yet deal with failures %d", err);

			/* This worked; hook the device to the bus' device list */
			struct USB_BUS* bus = usb_dev->usb_bus;
			mutex_lock(&bus->bus_mutex);
			LIST_APPEND(&bus->bus_devices, usb_dev);
			mutex_unlock(&bus->bus_mutex);
		}
	}
}

void
usbbus_init()
{
	sem_init(&usbbus_semaphore, 0);
	LIST_INIT(&usbbus_pendingqueue);
	mutex_init(&usbbus_mutex, "usbbus");
	LIST_INIT(&usbbus_busses);

	/*
	 * Create a kernel thread to handle USB device attachments. We use a thread for this
	 * since we can only attach one at the same time.
	 */
	kthread_init(&usbbus_thread, "usbbus", &usb_bus_thread, NULL);
	thread_resume(&usbbus_thread);
}

static struct DRIVER drv_usbbus = {
	.name = "usbbus",
	.drv_probe = NULL,
	.drv_attach = usbbus_attach,
};

DRIVER_PROBE(usbbus)
DRIVER_PROBE_BUS(ohci)
DRIVER_PROBE_BUS(uhci)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
