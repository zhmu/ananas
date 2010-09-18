#include <ananas/device.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include <ananas/mm.h>

static int
ofw_attach(device_t dev)
{
	return 0;
}

static ssize_t
ofw_write(device_t dev, const void* data, size_t len, off_t offset)
{
	uint8_t* ptr = (uint8_t*)data;
	const char* ch = (const char*)data;
	size_t amount = 0;

	for (amount = 0; amount < len; amount++, ch++)
		ofw_putch(*ch);
	return amount;
}

static ssize_t
ofw_read(device_t dev, void* data, size_t len, off_t offset)
{
	return 0;
}

struct DRIVER drv_ofwconsole = {
	.name					= "ofwconsole",
	.drv_probe		= NULL,
	.drv_attach		= ofw_attach,
	.drv_write		= ofw_write,
	.drv_read     = ofw_read
};

DRIVER_PROBE(ofwconsole)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
