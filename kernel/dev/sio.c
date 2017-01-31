#include <ananas/x86/io.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/tty.h>
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

static irqresult_t
sio_irq(device_t dev, void* context)
{
	struct SIO_PRIVDATA* privdata = (struct SIO_PRIVDATA*)dev->privdata;

	uint8_t ch = inb(privdata->io_port + SIO_REG_DATA);
	privdata->buffer[privdata->buffer_writepos] = ch;
	privdata->buffer_writepos = (privdata->buffer_writepos + 1) % SIO_BUFFER_SIZE;

	/* XXX signal consumers - this is a hack */
	if (console_tty != NULL && tty_get_inputdev(console_tty) == dev)
		tty_signal_data();

	return IRQ_RESULT_PROCESSED;
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

	/* SIO is so simple that a plain ISR will do */
	errorcode_t err = irq_register((uintptr_t)res_irq, dev, sio_irq, IRQ_TYPE_ISR, NULL);
	if (err != ANANAS_ERROR_NONE) {
		kfree(privdata);
		return err;
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
	return ananas_success();
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
	return ananas_success();
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
	return ananas_success();
}

static errorcode_t
sio_probe(device_t dev)
{
	struct RESOURCE* res = device_get_resource(dev, RESTYPE_PNP_ID, 0);
	if (res != NULL && res->base == 0x0501) /* PNP0501: 16550A-compatible COM port */
		return ananas_success();
	return ANANAS_ERROR(NO_DEVICE);
}

struct DRIVER drv_sio = {
	.name					= "sio",
	.drv_probe		= sio_probe,
	.drv_attach		= sio_attach,
	.drv_write		= sio_write,
	.drv_read     = sio_read
};

DRIVER_PROBE(sio)
DRIVER_PROBE_BUS(acpi)
DRIVER_PROBE_END()

DEFINE_CONSOLE_DRIVER(drv_sio, 0, CONSOLE_FLAG_INOUT);

/* vim:set ts=2 sw=2: */
