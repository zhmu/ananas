#include <machine/io.h>
#include <sys/device.h>
#include <sys/lib.h>
#include <sys/mm.h>

struct SIO_PRIVDATA {
	uint32_t io_port;
};

static int
sio_attach(device_t dev)
{
	void* res = device_alloc_resource(dev, RESTYPE_IO, 7);
	if (res == NULL)
		return 1; /* XXX */

	struct SIO_PRIVDATA* privdata = kmalloc(sizeof(struct SIO_PRIVDATA));
	privdata->io_port = (uint32_t)(uintptr_t)res;

	dev->privdata = privdata;
	return 0;
}

static ssize_t
sio_write(device_t dev, const char* data, size_t len)
{
	struct SIO_PRIVDATA* privdata = (struct SIO_PRIVDATA*)dev->privdata;
	size_t amount;

	for (amount = 0; amount < len; amount++, data++) {
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
		: : "b" (*data), "c" (privdata->io_port));
		/* XXX KLUDGE */
		if (len == 1 && *data == '\n') {
			char tmp = '\r';
			sio_write(dev, &tmp, 1);
		}
	}
	return amount;
}

struct DRIVER drv_sio = {
	.name					= "sio",
	.drv_probe		= NULL,
	.drv_attach		= sio_attach,
	.drv_write		= sio_write
};

DRIVER_PROBE(sio)
DRIVER_PROBE_BUS(isa)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
