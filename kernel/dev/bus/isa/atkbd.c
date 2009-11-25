#include "device.h"
#include "irq.h"
#include "lib.h"

uint32_t atkbd_port = 0;

void
atkbd_irq(device_t dev)
{
	uint8_t scancode = inb(atkbd_port);
	kprintf("kbd[%u]", scancode);
}

static int
atkbd_attach(device_t dev)
{
	void* res_io  = device_alloc_resource(dev, RESTYPE_IO, 7);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return 1; /* XXX */
	atkbd_port = (uint32_t)res_io;

	if (!irq_register((uint32_t)res_irq, dev, atkbd_irq))
		return 1;

	return 0;
}

static ssize_t
atkbd_read(device_t dev, const char* data, size_t len)
{
	return 0;
}

struct DRIVER drv_atkbd = {
	.name       = "atkbd",
	.drv_probe  = NULL,
	.drv_attach = atkbd_attach,
	.drv_read   = atkbd_read
};

DRIVER_PROBE(atkbd)
DRIVER_PROBE_BUS(isa)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
