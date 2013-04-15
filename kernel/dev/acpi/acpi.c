#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include <ananas/x86/acpi.h>
#include "acpica/acpi.h"
#include "acpi.h"
#include "acpi_resource.h"

TRACE_SETUP;

static ACPI_STATUS
acpi_probe_device(ACPI_HANDLE ObjHandle, UINT32 Level, void* Context, void** ReturnValue)
{
	ACPI_OBJECT_TYPE type;
	if (!ACPI_SUCCESS(AcpiGetType(ObjHandle, &type)))
		return AE_OK;

	/* Grab the device name; this is handy for debugging */
	char name[256];
	ACPI_BUFFER buf;
	buf.Length = sizeof(name);
	buf.Pointer = name;
	if (ACPI_FAILURE(AcpiGetName(ObjHandle, ACPI_FULL_PATHNAME, &buf)))
		return AE_OK;

	/* Make a device for us, we won't yet know the driver tho */
	device_t bus = Context;
	device_t dev = device_alloc(bus, NULL);

	/* Fetch the device resources */
	ACPI_STATUS status = AE_OK;
	if (ACPI_SUCCESS(acpi_process_resources(ObjHandle, dev))) {
		/*
		 * And see if we can attach this device to something - XXX As in the PCI
		 * case, we need to get better support for this stuff instead of looking
		 * in the device stuff's internal structures...
		 */
		extern struct DEVICE_PROBE probe_queue;

		int device_attached = 0;
		DQUEUE_FOREACH(&probe_queue, p, struct PROBE) {
			/* See if the device lives on our bus */
			int exists = 0;
			for (const char** curbus = p->bus; *curbus != NULL; curbus++) {
				if (strcmp(*curbus, bus->name) == 0) {
					exists = 1;
					break;
				}
			}
			if (!exists)
				continue;

			/* This device may work - give it a chance to attach */
			dev->driver = p->driver;
			strcpy(dev->name, dev->driver->name);
			dev->unit = dev->driver->current_unit++;
			errorcode_t err = device_attach_single(dev);
			if (err == ANANAS_ERROR_NONE) {
				/* This worked; use the next unit for the new device */
				device_attached++;
				break;
			} else {
				/* No luck, revert the unit number */
				dev->driver->current_unit--;
			}
		}

		/*
		 * If we managed to attach the device, ignore going deeper during the ACPI
		 * bus walk - the device is expected to handle this. Otherwise, throw away
		 * the device we created and continue.
		 */
		if (device_attached) {
			status = AE_CTRL_DEPTH;
		} else {
			device_free(dev);
		}
	}

	return status;
}

static errorcode_t
acpi_probe(device_t dev)
{
	/*
	 * XXX This means we'll end up finding the root pointer twice if it exists
	 * since the attach function will do it too...
	 */
	ACPI_SIZE TableAddress;
	if (AcpiFindRootPointer(&TableAddress) != AE_OK)
		return ANANAS_ERROR(NO_DEVICE);
	return ANANAS_ERROR_OK;
}

static errorcode_t
acpi_attach(device_t dev)
{
	ACPI_STATUS status;

	/* Initialize ACPI subsystem */
	status = AcpiInitializeSubsystem();
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/* Initialize tablde manager and fetch all tables */
	status = AcpiInitializeTables(NULL, 16, FALSE);
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/* Create ACPI namespace from ACPI tables */
	status = AcpiLoadTables();
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/* Enable ACPI hardware */
	status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/* Complete ACPI namespace object initialization */
	status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status))
		return ANANAS_ERROR(NO_DEVICE);

	/*
	 * Now enumerate through all ACPI devices and see what we can find.
	 */
	AcpiWalkNamespace(ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, acpi_probe_device, NULL, dev, NULL);

	return ANANAS_ERROR_OK;
}

void
acpi_init()
{
	/*
	 * Preform early initialization; this is used by the SMP code because it
	 * needs the MADT table before the ACPI is fully initialized.
	 */
	AcpiInitializeTables(NULL, 2, TRUE);
}

struct DRIVER drv_acpi = {
	.name				= "acpi",
	.drv_probe	= acpi_probe,
	.drv_attach	= acpi_attach,
	.drv_write	= NULL
};

DRIVER_PROBE(acpi)
DRIVER_PROBE_BUS(corebus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
