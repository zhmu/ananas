#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/tty.h>
#include <ananas/console.h>
#include <ananas/x86/io.h>
#include "options.h"

uint32_t atkbd_port = 0;

/* Mapping of a scancode to an ASCII key value */
static uint8_t atkbd_keymap[128] = {
	/* 00-07 */    0, 0x1b, '1',  '2',  '3', '4', '5', '6',
	/* 08-0f */  '7',  '8', '9',  '0',  '-', '=',   8,   9,
	/* 10-17 */  'q',  'w', 'e',  'r',  't', 'y', 'u', 'i',
	/* 18-1f */  'o',  'p', '[',  ']', 0x0d,   0, 'a', 's', 
	/* 20-27 */  'd',  'f', 'g',  'h',  'j', 'k', 'l', ';',
	/* 28-2f */  '\'',  '`',   0, '\\', 'z', 'x', 'c', 'v',
	/* 30-37 */  'b',  'n', 'm',  ',',  '.', '/',   0, '*',
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

/* Mapping of a scancode to an ASCII key value, if shift is active */
static uint8_t atkbd_keymap_shift[128] = {
	/* 00-07 */    0, 0x1b, '!',  '@',  '#', '$', '%', '^',
	/* 08-0f */  '&',  '*', '(',  ')',  '_', '+',   8,   9,
	/* 10-17 */  'Q',  'W', 'E',  'R',  'T', 'Y', 'U', 'I',
	/* 18-1f */  'O',  'P', '{',  '}', 0x0d,   0, 'A', 'S', 
	/* 20-27 */  'D',  'F', 'G',  'H',  'J', 'K', 'L', ':',
	/* 28-2f */  '"',  '~',   0,  '|',  'Z', 'X', 'C', 'V',
	/* 30-37 */  'B',  'N', 'M',  '<',  '>', '?',   0, '*',
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
#define ATKBD_FLAG_SHIFT 1
#define ATKBD_FLAG_CONTROL 2
#define ATKBD_FLAG_ALT 4

uint8_t atkbd_buffer[ATKBD_BUFFER_SIZE];
uint8_t atkbd_buffer_readpos = 0;
uint8_t atkbd_buffer_writepos = 0;
uint8_t atkbd_flags = 0;

void
atkbd_irq(device_t dev)
{
	uint8_t scancode = inb(atkbd_port);
	if ((scancode & 0x7f) == 0x2a /* LSHIFT */ ||
	    (scancode & 0x7f) == 0x36 /* RSHIFT */) {
		if (scancode & 0x80)
			atkbd_flags &= ~ATKBD_FLAG_SHIFT;
		else
			atkbd_flags |=  ATKBD_FLAG_SHIFT;
		return;
	}
	/* right-alt is 0xe0 0x38 but that does not matter */
	if ((scancode & 0x7f) == 0x38 /* ALT */) {
		if (scancode & 0x80)
			atkbd_flags &= ~ATKBD_FLAG_ALT;
		else
			atkbd_flags |=  ATKBD_FLAG_ALT;
		return;
	}
	/* right-control is 0xe0 0x1d but that does not matter */
	if ((scancode & 0x7f) == 0x1d /* CONTROL */) {
		if (scancode & 0x80)
			atkbd_flags &= ~ATKBD_FLAG_CONTROL;
		else
			atkbd_flags |=  ATKBD_FLAG_CONTROL;
		return;
	}
	if (scancode & 0x80)
		return;

#ifdef KDB
	if ((atkbd_flags == (ATKBD_FLAG_CONTROL | ATKBD_FLAG_SHIFT)) && scancode == 0x29 /* tilde */) {
		kdb_enter("keyboard sequence");
		return;
	}
#endif

	uint8_t ascii = ((atkbd_flags & ATKBD_FLAG_SHIFT) ? atkbd_keymap_shift : atkbd_keymap)[scancode];
	if (ascii == 0)
		return;

	atkbd_buffer[atkbd_buffer_writepos] = ascii;
	atkbd_buffer_writepos = (atkbd_buffer_writepos + 1) % ATKBD_BUFFER_SIZE;

	/* XXX signal consumers - this is a hack */
	tty_signal_data(console_tty);
}

static int
atkbd_attach(device_t dev)
{
	void* res_io  = device_alloc_resource(dev, RESTYPE_IO, 7);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return 1; /* XXX */
	atkbd_port = (uintptr_t)res_io;

	if (!irq_register((uintptr_t)res_irq, dev, atkbd_irq))
		return 1;
	
	/*
	 * Ensure the keyboard's input buffer is empty; this will cause it to
	 * send IRQ's to us.
	 */
	inb(atkbd_port);

	return 0;
}

static int
atkbd_read(device_t dev, void* data, size_t* len, off_t off)
{
	size_t returned = 0, to_read = *len;

	while (to_read-- > 0) {
		if (atkbd_buffer_readpos == atkbd_buffer_writepos)
			break;

		*(uint8_t*)(data + returned++) = atkbd_buffer[atkbd_buffer_readpos];
		atkbd_buffer_readpos = (atkbd_buffer_readpos + 1) % ATKBD_BUFFER_SIZE;
	}
	*len = returned;
	return ANANAS_ERROR_OK;
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
