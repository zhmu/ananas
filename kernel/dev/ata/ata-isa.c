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

	struct ATA_PRIVDATA* priv = kmalloc(sizeof(struct ATA_PRIVDATA));
	priv->io_port = (uint32_t)(uintptr_t)res_io;
	/* XXX this is a hack - at least, until we properly support multiple resources */
	if (priv->io_port == 0x170) {
		priv->io_port2 = (uint32_t)0x374;
	} else if (priv->io_port == 0x1f0) {
		priv->io_port2 = (uint32_t)0x3f4;
	} else {
		device_printf(dev, "couldn't determine second I/O range");
		return ANANAS_ERROR(NO_RESOURCE);
	}
	QUEUE_INIT(&priv->requests);
	dev->privdata = priv;

	/* Ensure there's something living at the I/O addresses */
	if (inb(priv->io_port + ATA_REG_STATUS) == 0xff) return 1;

	if (!irq_register((uintptr_t)res_irq, dev, ata_irq))
		return ANANAS_ERROR(NO_RESOURCE);

	/* reset the control register - this ensures we receive interrupts */
	outb(priv->io_port + ATA_REG_DEVCONTROL, 0);
	return ANANAS_ERROR_OK;
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
