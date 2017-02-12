#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/kmem.h>
#include <ananas/tty.h>
#include <ananas/thread.h>
#include <ananas/trace.h>
#include <ananas/vm.h>
#include "options.h"

TRACE_SETUP;

static void device_print_attachment(device_t dev);

static spinlock_t spl_devicequeue = SPINLOCK_DEFAULT_INIT;
static struct DEVICE_QUEUE device_queue;
static struct DEVICE_PROBE probe_queue; /* XXX not locked yet */

/* Note: drv = NULL will be used if the driver isn't yet known! */
device_t
device_alloc(device_t bus, driver_t drv)
{
	auto dev = new DEVICE;
	memset(dev, 0, sizeof(struct DEVICE));
	dev->driver = drv;
	dev->parent = bus;
	sem_init(&dev->waiters, 1);

	if (drv != NULL) {
		strcpy(dev->name, drv->name);
		dev->unit = drv->current_unit++;
	}

	return dev;
}

/* Clones a device; it will get a new unit number */
device_t
device_clone(device_t dev)
{
	device_t new_dev = device_alloc(dev->parent, dev->driver);
	new_dev->d_resourceset = dev->d_resourceset;
	return new_dev;
}

errorcode_t
device_detach(device_t dev)
{
	driver_t driver = dev->driver;
	if (driver != NULL && driver->drv_detach != NULL) {
		errorcode_t err = driver->drv_detach(dev);
		ANANAS_ERROR_RETURN(err);
	}

	return ananas_success();
}

void
device_free(device_t dev)
{
	/* XXX clear waiters; should we signal them? */

	spinlock_lock(&spl_devicequeue);
	LIST_REMOVE(&device_queue, dev);
	spinlock_unlock(&spl_devicequeue);
	kfree(dev);
}

static void
device_add_to_tree(device_t dev)
{
	spinlock_lock(&spl_devicequeue);
	LIST_FOREACH(&device_queue, dq_dev, struct DEVICE) {
		if (dq_dev == dev)
			goto skip;
	}
	LIST_APPEND(&device_queue, dev);
skip:
	spinlock_unlock(&spl_devicequeue);
}

errorcode_t
device_attach_single(device_t dev)
{
	driver_t driver = dev->driver;

	if (driver->drv_probe != NULL) {
		/*
		 * This device has a probe function; we must call it to figure out
	 	 * whether the device actually exists or we're about to attach
		 * something out of thin air here...
		 */
		errorcode_t err = driver->drv_probe(dev);
		ANANAS_ERROR_RETURN(err);
	}

	/*
	 * XXX This is a kludge: it prevents us from displaying attach information for drivers
	 * that will be initialize outside the probe tree (such as the console which will be
	 * initialized as soon as possible.
	 */
	if (dev->parent != NULL)
		device_print_attachment(dev);
	if (driver->drv_attach != NULL) {
		errorcode_t err = driver->drv_attach(dev);
		ANANAS_ERROR_RETURN(err);
	}

	/* Hook the device up to the tree */
	device_add_to_tree(dev);

	/* Attempt to attach child devices, if any */
	if (driver->drv_attach_children != NULL)
		driver->drv_attach_children(dev);
	device_attach_bus(dev);

	return ananas_success();
}

static void
device_print_attachment(device_t dev)
{
	KASSERT(dev->parent != NULL, "can't print device which doesn't have a parent bus");
	kprintf("%s%u on %s%u ", dev->name, dev->unit, dev->parent->name, dev->parent->unit);
	dev->d_resourceset.Print();
	kprintf("\n");
}

static int
device_must_probe_on_bus(const struct PROBE* p, const device_t bus)
{
	const char* ptr = p->busses;
	while(*ptr != '\0') {
		const char* next = strchr(ptr, ',');
		if (next == NULL)
			next = strchr(ptr, '\0');

		if (strncmp(ptr, bus->name, next - ptr) == 0)
			return 1;

		ptr = next + 1;
	}
	return 0;
}

/*
 * Attaches a single device, by handing it off to all possible drivers (first
 * one wins). Succeeds if a driver accepted the device, in which case the
 * device name and unit will be updated.
 */
errorcode_t
device_attach_child(device_t dev)
{
	if (LIST_EMPTY(&probe_queue))
		return ANANAS_ERROR(NO_DEVICE);

	device_t bus = dev->parent;
	LIST_FOREACH_SAFE(&probe_queue, p, struct PROBE) {
		if (!device_must_probe_on_bus(p, bus))
			continue; /* bus cannot contain this device */

		driver_t driver = p->driver;
		KASSERT(driver != NULL, "matched a probe device without a driver!");

		/* Hook the device to this driver and try to attach it */
		dev->driver = driver;
		strcpy(dev->name, driver->name);
		dev->unit = driver->current_unit++;

		errorcode_t err = device_attach_single(dev);
		if (ananas_is_success(err))
			return err;

		/* Attach failed */
		strcpy(dev->name, "???");
		dev->driver = NULL;
		driver->current_unit--;
	}

	return ANANAS_ERROR(NO_DEVICE);
}

/*
 * Attaches children on a given bus - assumes the bus is already set up. May
 * attach multiple devices or none at all.
 *
 * XXX contains hacks for the input/output console device stuff (skips
 * attaching them, as they are already at the point this is run)
 */
void
device_attach_bus(device_t bus)
{
	if (LIST_EMPTY(&probe_queue))
		return;

	/*
	 * Fetch TTY devices; we need them to report the device that is already
	 * attached at this point.
	 */
	device_t input_dev = tty_get_inputdev(console_tty);
	device_t output_dev = tty_get_outputdev(console_tty);
	LIST_FOREACH_SAFE(&probe_queue, p, struct PROBE) {
		if (!device_must_probe_on_bus(p, bus))
			continue; /* bus cannot contain this device */

		driver_t driver = p->driver;
		KASSERT(driver != NULL, "matched a probe device without a driver!");

		/*
		 * If we found the driver for the in- or output driver, display it.
		 *
		 * XXX we will any extra units here, maybe we should re-think that? The entire
		 *     tty-device-stuff is a hack anyway...
		 */
		if (input_dev != NULL && input_dev->driver == driver) {
			input_dev->parent = bus;
			device_print_attachment(input_dev);
			device_add_to_tree(input_dev);
			continue;
		}
		if (output_dev != NULL && output_dev->driver == driver && input_dev != output_dev) {
			output_dev->parent = bus;
			device_print_attachment(output_dev);
			device_add_to_tree(output_dev);
			continue;
		}

		/* Hook the device to this driver and try to attach it */
		device_t dev = device_alloc(bus, driver);
		errorcode_t err = device_attach_single(dev);
		if (ananas_is_failure(err))
			device_free(dev); /* attach failed; discard the device */
	}
}

static errorcode_t
device_init()
{
	LIST_INIT(&device_queue);

	/*
	 * First of all, create the core bus; this is as bare to the metal as it
	 * gets.
	 */
	device_t corebus = new DEVICE;
	memset(corebus, 0, sizeof(struct DEVICE));
	strcpy(corebus->name, "corebus");
	device_attach_bus(corebus);
	device_add_to_tree(corebus);
	return ananas_success();
}

INIT_FUNCTION(device_init, SUBSYSTEM_DEVICE, ORDER_LAST);

struct DEVICE*
device_find(const char* name)
{
	char* ptr = (char*)name;
	while (*ptr != '\0' && (*ptr < '0' || *ptr > '9')) ptr++;
	int unit = (*ptr != '\0') ? strtoul(ptr, NULL, 10) : 0;

	LIST_FOREACH(&device_queue, dev, struct DEVICE) {
		if (!strncmp(dev->name, name, ptr - name) && dev->unit == unit)
			return dev;
	}
	return NULL;
}

errorcode_t
device_write(device_t dev, const void* buf, size_t* len, off_t offset)
{
	KASSERT(dev->driver != NULL, "device_write() without a driver");
	KASSERT(dev->driver->drv_write != NULL, "device_write() without drv_write");

	return dev->driver->drv_write(dev, buf, len, offset);
}

errorcode_t
device_bwrite(device_t dev, struct BIO* bio)
{
	KASSERT(dev->driver != NULL, "device_bwrite() without a driver");
	KASSERT(dev->driver->drv_bwrite != NULL, "device_bwrite() without drv_bwrite");

	return dev->driver->drv_bwrite(dev, bio);
}

errorcode_t
device_read(device_t dev, void* buf, size_t* len, off_t offset)
{
	KASSERT(dev->driver != NULL, "device_read() without a driver");
	KASSERT(dev->driver->drv_read != NULL, "device_read() without drv_read");

	return dev->driver->drv_read(dev, buf, len, offset);
}

errorcode_t
device_bread(device_t dev, struct BIO* bio)
{
	KASSERT(dev->driver != NULL, "device_bread() without a driver");
	KASSERT(dev->driver->drv_bread != NULL, "device_bread() without drv_bread");

	return dev->driver->drv_bread(dev, bio);
}

void
device_printf(device_t dev, const char* fmt, ...)
{
#define DEVICE_PRINTF_BUFSIZE 256

	char buf[DEVICE_PRINTF_BUFSIZE];

	snprintf(buf, sizeof(buf), "%s%u: ", dev->name, dev->unit);

	va_list va;
	va_start(va, fmt);
	vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 2, fmt, va);
	buf[sizeof(buf) - 2] = '\0';
	strcat(buf, "\n");
	va_end(va);

	console_putstring(buf);
}

struct DEVICE_QUEUE*
device_get_queue()
{
	return &device_queue;
}

errorcode_t
device_register_probe(struct PROBE* p)
{
	LIST_FOREACH(&probe_queue, pqi, struct PROBE) {
		if (pqi->driver == p->driver) {
			/* Duplicate driver */
			return ANANAS_ERROR(FILE_EXISTS);
		}
	}
	LIST_APPEND(&probe_queue, p);
	return ananas_success();
}

errorcode_t
device_unregister_probe(struct PROBE* p)
{
	LIST_REMOVE(&probe_queue, p);
	return ananas_success();
}

void*
operator new(size_t len, device_t dev) throw()
{
	return kmalloc(len);
}

void*
operator new[](size_t len, device_t dev) throw()
{
	return kmalloc(len);
}

void
operator delete(void* p, device_t dev) throw()
{
	kfree(p);
}

void
operator delete[](void* p, device_t dev) throw()
{
	kfree(p);
}

#ifdef OPTION_KDB
static int
print_devices(device_t parent, int indent)
{
	int count = 0;
	LIST_FOREACH(&device_queue, dev, struct DEVICE) {
		if (dev->parent != parent)
			continue;
		for (int n = 0; n < indent; n++)
			kprintf(" ");
		kprintf("device %p: '%s' unit %u\n",
	 	 dev, dev->name, dev->unit);
		count += print_devices(dev, indent + 1) + 1;
	}
	return count;
}

KDB_COMMAND(devices, NULL, "Displays a list of all devices")
{
	int count = print_devices(NULL, 0);

	/* See if we have printed everything; if not, our structure is wrong and we should fix this */
	int n = 0;
	LIST_FOREACH(&device_queue, dev, struct DEVICE) {
		n++;
	}
	if (n != count)
		kprintf("Warning: %d extra device(s) unreachable by walking the device chain!\n", n - count);
}

KDB_COMMAND(devdump, "s:devname", "Displays debugging dump of a device")
{
	struct DEVICE* dev = device_find(arg[1].a_u.u_string);
	if (dev == NULL) {
		kprintf("device not found\n");
		return;
	}

	if (dev->driver == NULL || dev->driver->drv_dump == NULL) {
		kprintf("dump not supported\n");
		return;
	}

	dev->driver->drv_dump(dev);
}
#endif

/* vim:set ts=2 sw=2: */
