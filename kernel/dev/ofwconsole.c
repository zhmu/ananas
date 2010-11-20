#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ofw.h>

static errorcode_t
ofwconsole_attach(device_t dev)
{
	return ANANAS_ERROR_OK;
}

static errorcode_t
ofwconsole_write(device_t dev, const void* data, size_t* len, off_t offset)
{
	uint8_t* ptr = (uint8_t*)data;
	const char* ch = (const char*)data;
	size_t left = *len;

	while (left > 0) {
		ofw_putch(*ch);
		ch++; left--;
	}
	return ANANAS_ERROR_OK;
}

static errorcode_t
ofwconsole_read(device_t dev, void* data, size_t* len, off_t offset)
{
	*len = 0;
	return ANANAS_ERROR_OK;
}

struct DRIVER drv_ofwconsole = {
	.name					= "ofwconsole",
	.drv_probe		= NULL,
	.drv_attach		= ofwconsole_attach,
	.drv_write		= ofwconsole_write,
	.drv_read     = ofwconsole_read
};

DRIVER_PROBE(ofwconsole)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
