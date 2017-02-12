#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include <ananas/x86/acpi.h>
#include "acpica/acpi.h"
#include "acpi.h"
#include "acpi_resource.h"

TRACE_SETUP;

static ACPI_STATUS
acpi_attach_device(ACPI_HANDLE ObjHandle, UINT32 Level, void* Context, void** ReturnValue)
{
	ACPI_OBJECT_TYPE type;
	if (ACPI_FAILURE(AcpiGetType(ObjHandle, &type)))
		return AE_OK;

	/* Grab the device name; this is handy for debugging */
	char name[256];
	ACPI_BUFFER buf;
	buf.Length = sizeof(name);
	buf.Pointer = name;
	if (ACPI_FAILURE(AcpiGetName(ObjHandle, ACPI_FULL_PATHNAME, &buf)))
		return AE_OK;

	/* Make a device for us, we won't yet know the driver tho */
	device_t bus = static_cast<device_t>(Context);
	device_t dev = device_alloc(bus, NULL);

	/* Fetch the device resources... */
	if (ACPI_SUCCESS(acpi_process_resources(ObjHandle, dev))) {
		/* ... and see if we can attach this device to something */
		if (ananas_is_failure(device_attach_child(dev))) {
			/*
			 * Unable to attach - do not return failure here, we need to walk
			 * deeper in the tree (ACPI won't list the PCI devices themselves,
			 * but it does provide an overview of the ISA stuff which we can't
			 * find otherwise)
			 */
			device_free(dev);
		}
	}

	return AE_OK;
}

static errorcode_t
acpi_probe(Ananas::ResourceSet& resourceSet)
{
	/*
	 * XXX This means we'll end up finding the root pointer twice if it exists
	 * since the attach function will do it too...
	 */
	ACPI_SIZE TableAddress;
	if (AcpiFindRootPointer(&TableAddress) != AE_OK)
		return ANANAS_ERROR(NO_DEVICE);
	return ananas_success();
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
	AcpiWalkNamespace(ACPI_TYPE_ANY, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, acpi_attach_device, NULL, dev, NULL);

	return ananas_success();
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
