#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include "acpica/acpi.h"
#include "acpi.h"
#include "acpi_resource.h"

TRACE_SETUP;

static errorcode_t
acpi_pcihb_probe(device_t dev)
{
	struct RESOURCE* res = device_get_resource(dev, RESTYPE_PNP_ID, 0);
	if (res != NULL && res->base == 0x0a03 /* PNP0A03: PCI Bus */)
		return ANANAS_ERROR_NONE;
	return ANANAS_ERROR(NO_DEVICE);
}

static errorcode_t
acpi_pcihb_attach(device_t dev)
{
	return ANANAS_ERROR_NONE;
}

struct DRIVER drv_acpi_pcihb = {
	.name				= "acpi-pcihb",
	.drv_probe	= acpi_pcihb_probe,
	.drv_attach	= acpi_pcihb_attach,
	.drv_write	= NULL
};

DRIVER_PROBE(acpi_pcihb)
DRIVER_PROBE_BUS(acpi)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
