#include <ananas/x86/io.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <ananas/x86/sio.h>

#define SIO_BUFFER_SIZE	16

TRACE_SETUP;

struct SIO_PRIVDATA {
	uint32_t io_port;
	/* Incoming data buffer */
	uint8_t buffer[SIO_BUFFER_SIZE];
	uint8_t buffer_readpos;
	uint8_t buffer_writepos;
};

static void
sio_irq(device_t dev)
{
	struct SIO_PRIVDATA* privdata = (struct SIO_PRIVDATA*)dev->privdata;

	uint8_t ch = inb(privdata->io_port + SIO_REG_DATA);
	privdata->buffer[privdata->buffer_writepos] = ch;
	privdata->buffer_writepos = (privdata->buffer_writepos + 1) % SIO_BUFFER_SIZE;
}

static errorcode_t
sio_attach(device_t dev)
{
	void* res_io = device_alloc_resource(dev, RESTYPE_IO, 7);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	struct SIO_PRIVDATA* privdata = kmalloc(sizeof(struct SIO_PRIVDATA));
	privdata->io_port = (uint32_t)(uintptr_t)res_io;

	dev->privdata = privdata;

	if (!irq_register((uintptr_t)res_irq, dev, sio_irq)) {
		kfree(privdata);
		return ANANAS_ERROR(NO_RESOURCE);
	}

	/*
	 * Wire up the serial port for sensible defaults.
	 */
	outb(privdata->io_port + SIO_REG_IER, 0);			/* Disables interrupts */
	outb(privdata->io_port + SIO_REG_LCR, 0x80);	/* Enable DLAB */
	outb(privdata->io_port + SIO_REG_DATA, 0xc);	/* Divisor low byte (9600 baud) */
	outb(privdata->io_port + SIO_REG_IER,  0);		/* Divisor hi byte */
	outb(privdata->io_port + SIO_REG_LCR, 3);			/* 8N1 */
	outb(privdata->io_port + SIO_REG_FIFO, 0xc7);	/* Enable/clear FIFO (14 bytes) */
	outb(privdata->io_port + SIO_REG_IER, 0x01);	/* Enable interrupts (recv only) */
	return ANANAS_ERROR_OK;
}

static errorcode_t
sio_write(device_t dev, const void* data, size_t* len, off_t offset)
{
	struct SIO_PRIVDATA* privdata = (struct SIO_PRIVDATA*)dev->privdata;
	const char* ch = (const char*)data;

	for (size_t n = 0; n < *len; n++, ch++) {
		while ((inb(privdata->io_port + SIO_REG_LSR) & 0x20) == 0);
		outb(privdata->io_port + SIO_REG_DATA, *ch);
	}
	return ANANAS_ERROR_OK;
}

static errorcode_t
sio_read(device_t dev, void* data, size_t* len, off_t offset)
{
	struct SIO_PRIVDATA* privdata = (struct SIO_PRIVDATA*)dev->privdata;
	size_t returned = 0, left = *len;
	char* buf = (char*)data;

	while (left-- > 0) {
		if (privdata->buffer_readpos == privdata->buffer_writepos)
			break;

		buf[returned++] = privdata->buffer[privdata->buffer_readpos];
		privdata->buffer_readpos = (privdata->buffer_readpos + 1) % SIO_BUFFER_SIZE;
	}
	*len = returned;
	return ANANAS_ERROR_OK;
}

struct DRIVER drv_sio = {
	.name					= "sio",
	.drv_probe		= NULL,
	.drv_attach		= sio_attach,
	.drv_write		= sio_write,
	.drv_read     = sio_read
};

DRIVER_PROBE(sio)
DRIVER_PROBE_BUS(isa)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
