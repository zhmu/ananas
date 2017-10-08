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
#include <ananas/error.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/thread.h"
#include "usb-bus.h"
#include "usb-device.h"

namespace Ananas {
namespace USB {

LIST_DEFINE(USBBusses, Bus);

namespace {
thread_t usbbus_thread;
semaphore_t usbbus_semaphore;
USBDevices usbbus_pendingqueue;
spinlock_t usbbus_spl_pendingqueue = SPINLOCK_DEFAULT_INIT; /* protects usbbus_pendingqueue */
USBBusses usbbus_busses;
mutex_t usbbus_mutex; /* protects usbbus_busses */
} // unnamed namespace

void
ScheduleAttach(USBDevice& usb_dev)
{
	/* Add the device to our queue */
	spinlock_lock(&usbbus_spl_pendingqueue);
	LIST_APPEND(&usbbus_pendingqueue, &usb_dev);
	spinlock_unlock(&usbbus_spl_pendingqueue);

	/* Wake up our thread */
	sem_signal(&usbbus_semaphore);
}

errorcode_t
Bus::Attach()
{
	mutex_init(&bus_mutex, "usbbus");
	LIST_INIT(&bus_devices);
	bus_NeedsExplore = true;

	/*
	 * Create the root hub device; it will handle all our children - the HCD may
	 * need to know about this as the root hub may be polling...
	  */
	auto roothub = new USBDevice(*this, nullptr, 0, USB_DEVICE_FLAG_ROOT_HUB);
	d_Parent->GetUSBDeviceOperations()->SetRootHub(*roothub);
	ScheduleAttach(*roothub);

	/* Register ourselves within the big bus list */
	mutex_lock(&usbbus_mutex);
	LIST_APPEND(&usbbus_busses, this);
	mutex_unlock(&usbbus_mutex);
	return ananas_success();
}

errorcode_t
Bus::Detach()
{
	panic("detach");
	return ananas_success();
}

int
Bus::AllocateAddress()
{
	/* XXX crude */
	static int cur_addr = 1;
	return cur_addr++;
}

void
Bus::ScheduleExplore()
{
	Lock();
	bus_NeedsExplore = true;
	Unlock();

	sem_signal(&usbbus_semaphore);
}

/* Must be called with lock held! */
void
Bus::Explore()
{
	AssertLocked();
	bus_NeedsExplore = false;

	LIST_FOREACH(&bus_devices, usb_dev, USBDevice) {
		Device* dev = usb_dev->ud_device;
		if (dev->GetUSBHubDeviceOperations() != nullptr)
			dev->GetUSBHubDeviceOperations()->HandleExplore();
	}
}

/* Must be called with lock held! */
errorcode_t
Bus::DetachHub(Hub& hub)
{
	AssertLocked();

	LIST_FOREACH_SAFE(&bus_devices, usb_dev, USBDevice) {
		if (usb_dev->ud_hub != &hub)
			continue;

		errorcode_t err = usb_dev->Detach();
		(void)err; // XXX what to do in this case?
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
			LIST_FOREACH(&usbbus_busses, bus, Bus) {
				bus->Lock();
				if (bus->bus_NeedsExplore)
					bus->Explore();
				bus->Unlock();
			}
			mutex_unlock(&usbbus_mutex);

			/* Handle attaching devices */
			spinlock_lock(&usbbus_spl_pendingqueue);
			if (LIST_EMPTY(&usbbus_pendingqueue)) {
				spinlock_unlock(&usbbus_spl_pendingqueue);
				break;
			}
			auto usb_dev = LIST_HEAD(&usbbus_pendingqueue);
			LIST_POP_HEAD(&usbbus_pendingqueue);
			spinlock_unlock(&usbbus_spl_pendingqueue);

			/*
			 * Note that attaching will block until completed, which is intentional
			 * as it ensures we will never attach more than one device in the system
			 * at any given time.
			 */
			errorcode_t err = usb_dev->Attach();
			KASSERT(ananas_is_success(err), "cannot yet deal with failures %d", err);

			/* This worked; hook the device to the bus' device list */
			Bus& bus = usb_dev->ud_bus;
			bus.Lock();
			LIST_APPEND(&bus.bus_devices, usb_dev);
			bus.Unlock();
		}
	}
}

namespace {

struct USBBus_Driver : public Ananas::Driver
{
	USBBus_Driver()
	 : Driver("usbbus")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "ohci,uhci";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return new Bus(cdp);
	}
};

errorcode_t
InitializeBus()
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
	return ananas_success();
}

} // unnamed namespace

REGISTER_DRIVER(USBBus_Driver)

INIT_FUNCTION(InitializeBus, SUBSYSTEM_DEVICE, ORDER_FIRST);

} // namespace USB
} // namespace Ananas

/* vim:set ts=2 sw=2: */
