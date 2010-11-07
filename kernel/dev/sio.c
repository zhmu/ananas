#include <machine/io.h>
#include <ananas/device.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/x86/sio.h>

#define SIO_BUFFER_SIZE	16

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

static int
sio_attach(device_t dev)
{
	void* res_io = device_alloc_resource(dev, RESTYPE_IO, 7);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return 1; /* XXX */

	struct SIO_PRIVDATA* privdata = kmalloc(sizeof(struct SIO_PRIVDATA));
	privdata->io_port = (uint32_t)(uintptr_t)res_io;

	dev->privdata = privdata;

	if (!irq_register((uintptr_t)res_irq, dev, sio_irq))
		return 1;

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
	return 0;
}

static ssize_t
sio_write(device_t dev, const void* data, size_t len, off_t offset)
{
	struct SIO_PRIVDATA* privdata = (struct SIO_PRIVDATA*)dev->privdata;
	size_t amount;
	const char* ch = (const char*)data;

	for (amount = 0; amount < len; amount++, ch++) {
		__asm(
			/* Poll the LSR to ensure we're not sending another character */
			"mov	%%ecx, %%edx\n"
			"addl	$5,%%edx\n"
	"z1f:\n"
			"in		%%dx,%%al\n"
			"test	$0x20, %%al\n"
			"jz	 	z1f\n"
			/* Place the character in the data register */
			"mov	%%ecx, %%edx\n"
			"mov	%%ebx, %%eax\n"
			"outb	%%al, %%dx\n"
			"mov	%%ecx, %%edx\n"
			"mov	%%ecx, %%edx\n"
			"addl	$5,%%edx\n"
	"z2f:\n"
			"in		%%dx,%%al\n"
			"test	$0x20, %%al\n"
			"jz	 	z2f\n"
		: : "b" (*ch), "c" (privdata->io_port));
	}
	return amount;
}

static ssize_t
sio_read(device_t dev, void* data, size_t len, off_t offset)
{
	struct SIO_PRIVDATA* privdata = (struct SIO_PRIVDATA*)dev->privdata;
	size_t returned = 0;
	char* buf = (char*)data;

	while (len-- > 0) {
		if (privdata->buffer_readpos == privdata->buffer_writepos)
			break;

		buf[returned++] = privdata->buffer[privdata->buffer_readpos];
		privdata->buffer_readpos = (privdata->buffer_readpos + 1) % SIO_BUFFER_SIZE;
	}
	return returned;
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
