#include "device.h"
#include "irq.h"
#include "lib.h"

uint32_t atkbd_port = 0;

/* Mapping of a scancode to an ASCII key value */
static uint8_t atkbd_keymap[128] = {
	/* 00-07 */    0, 0x1b, '1',  '2',  '3', '4', '5', '6',
	/* 08-0f */  '7',  '8', '9',  '0',  '-', '+',   8,   9,
	/* 10-17 */  'q',  'w', 'e',  'r',  't', 'y', 'u', 'i',
	/* 18-1f */  'o',  'p', '[',  ']', 0x0d,   0, 'a', 's', 
	/* 20-27 */  'd',  'f', 'g',  'h',  'j', 'k', 'l', ';',
	/* 28-2f */  ':',  '`',   0, 0x27,  'z', 'x', 'c', 'v',
	/* 30-37 */  'b',  'n', 'm',  ',',  '.', '/', '?', '*',
	/* 38-3f */    0,  ' ',   0,    0,    0,   0,   0,   0,
	/* 40-47 */    0,    0,   0,    0,    0,   0,   0, '7',
	/* 48-4f */  '8',  '9', '-',  '4',  '5', '6', '+', '1',
	/* 50-57 */  '2',  '3', '0',  '.',    0,   0, 'Z',   0,
	/* 57-5f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 60-67 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 68-6f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 70-76 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 77-7f */    0,    0,   0,    0,    0,   0,   0,   0
};

#define ATKBD_BUFFER_SIZE	16

uint8_t atkbd_buffer[ATKBD_BUFFER_SIZE];
uint8_t atkbd_buffer_readpos = 0;
uint8_t atkbd_buffer_writepos = 0;

void
atkbd_irq(device_t dev)
{
	uint8_t scancode = inb(atkbd_port);
	if (scancode & 0x80)
		return;

	uint8_t ascii = atkbd_keymap[scancode];
	if (ascii == 0)
		return;

	atkbd_buffer[atkbd_buffer_writepos] = ascii;
	atkbd_buffer_writepos = (atkbd_buffer_writepos + 1) % ATKBD_BUFFER_SIZE;
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
atkbd_read(device_t dev, char* data, size_t len)
{
	size_t returned = 0;

	while (len-- > 0) {
		if (atkbd_buffer_readpos == atkbd_buffer_writepos)
			break;

		data[returned++] = atkbd_buffer[atkbd_buffer_readpos];
		atkbd_buffer_readpos = (atkbd_buffer_readpos + 1) % ATKBD_BUFFER_SIZE;
	}
	return returned;
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
