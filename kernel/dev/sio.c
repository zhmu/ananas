#include "i386/io.h"
#include "device.h"
#include "lib.h"

#define SIO_PORT 0x3f8

static int
sio_attach(device_t dev)
{
	return 0;
}

static ssize_t
sio_write(device_t dev, const char* data, size_t len)
{
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
		: : "b" (*data), "c" (SIO_PORT));
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
DRIVER_PROBE_BUS(corebus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
