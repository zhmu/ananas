#include <ananas/types.h>
#include <ananas/dev/ata.h>
#include <ananas/x86/io.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <ananas/lib.h>

TRACE_SETUP;

static errorcode_t
ataisa_attach(device_t dev)
{
	void* res_io = device_alloc_resource(dev, RESTYPE_IO, 7);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	return ata_attach(dev, (uint32_t)res_io, (uint32_t)res_irq)
}

struct DRIVER drv_ataisa = {
	.name					= "ataisa",
	.drv_probe		= NULL,
	.drv_attach		= ataisa_attach,
	.drv_attach_children		= ata_attach_children,
	.drv_enqueue	= ata_enqueue,
	.drv_start		= ata_start
};

DRIVER_PROBE(ataisa)
DRIVER_PROBE_BUS(isa)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
