#include "console.h"
#include "device.h"
#include "lib.h"
#include "mm.h"
#include "console.inc"

/*
 * devprobe.inc will be generated by config and just lists all drivers we have
 * to probe.
 */
#include "devprobe.inc"

static device_t corebus = NULL;

device_t
device_alloc(driver_t drv)
{
	KASSERT(drv != NULL, "tried to allocate a NULL driver");

	device_t dev = kmalloc(sizeof(device_t));
	memset(dev, 0, sizeof(device_t));
	dev->driver = drv;
	strcpy(dev->name, drv->name);
	return dev;
}

int
device_attach_single(device_t* dev, device_t bus, driver_t driver)
{
	int result;

	*dev = device_alloc(driver);
	if (driver->drv_probe != NULL) {
		/*
		 * This device has a probe function; we must call it to figure out
	 	 * whether the device actually exists or we're about to attach
		 * something out of thin air here...
		 */
		result = driver->drv_probe(*dev);
		if (result)
			goto fail;
	}
	if (driver->drv_attach != NULL) {
		result = driver->drv_attach(*dev);
		if (result)
			goto fail;
	}

	return 0;

fail:
	kfree(*dev);
	*dev = NULL;
	return result;
}

/*
 * Handles attaching devices to a bus; may recursively call itself.
 */
void
device_attach_bus(device_t bus)
{
	for (struct PROBE** p = devprobe; *p != NULL; p++) {
		/* See if the device exists on this bus */
		int exists = 0;
		for (const char** curbus = (*p)->bus; *curbus != NULL; curbus++) {
			if (strcmp(*curbus, bus->name) == 0) {
				exists = 1;
				break;
			}
		}

		if (!exists)
			continue;

		/*
		 * OK, the device may be present on this bus; allocate a device for it.
		 */
		driver_t driver = (*p)->driver;
		KASSERT(driver != NULL, "matched a probe device without a driver!");

		/*
		 * If a console driver was specified, we need to skip attaching it
		 * again; we cheat and claim we attached it, yet we already did a long
		 * time ago ;-)
		 * 
		 */
#ifdef CONSOLE_DRIVER
		extern struct DRIVER CONSOLE_DRIVER;
		if (driver == &CONSOLE_DRIVER) {
			kprintf("%s on %s\n", console_dev->name, bus->name);
			continue;
		}
#endif

		device_t dev;
		int result = device_attach_single(&dev, bus, driver);
		if (result != 0)
			continue;

		/* We have a device; tell the user and see if anything lives on this bus */
		kprintf("%s on %s\n", dev->name, bus->name);
		device_attach_bus(dev);
	}
}

void
device_init()
{
	/*
	 * First of all, create the core bus; this is as bare to the metal as it
	 * gets.
	 */
	corebus = (device_t)kmalloc(sizeof(device_t));
	memset(corebus, 0, sizeof(device_t));
	strcpy(corebus->name, "corebus");
	device_attach_bus(corebus);
}

/* vim:set ts=2 sw=2: */
