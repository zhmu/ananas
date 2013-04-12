#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include "acpica/acpi.h"

TRACE_SETUP;

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

	return ANANAS_ERROR_OK;
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
