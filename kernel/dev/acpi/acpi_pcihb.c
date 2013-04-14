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
#if 0
	kprintf("res: %x, base=%x, id=%x", res, (res != NULL) ? res->base : 0, ACPI_PNP_ID("PNP0A03"));
#endif
	if (res != NULL && res->base == ACPI_PNP_ID("PNP0A03"))
		return ANANAS_ERROR_OK;
	return ANANAS_ERROR(NO_DEVICE);
}

static errorcode_t
acpi_pcihb_attach(device_t dev)
{
	return ANANAS_ERROR_OK;
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
