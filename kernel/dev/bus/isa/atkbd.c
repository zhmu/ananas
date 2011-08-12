#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/tty.h>
#include <ananas/trace.h>
#include <ananas/console.h>
#include <ananas/x86/io.h>
#include "options.h"

TRACE_SETUP;

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

struct ATKBD_PRIVDATA {
	int	 kbd_ioport;
	char kbd_buffer[ATKBD_BUFFER_SIZE];
	char kbd_buffer_readpos;
	char kbd_buffer_writepos;
	char kbd_flags;
};

void
atkbd_irq(device_t dev)
{
	struct ATKBD_PRIVDATA* priv = dev->privdata;

	uint8_t scancode = inb(priv->kbd_ioport);
	if ((scancode & 0x7f) == 0x2a /* LSHIFT */ ||
	    (scancode & 0x7f) == 0x36 /* RSHIFT */) {
		if (scancode & 0x80)
			priv->kbd_flags &= ~ATKBD_FLAG_SHIFT;
		else
			priv->kbd_flags |=  ATKBD_FLAG_SHIFT;
		return;
	}
	/* right-alt is 0xe0 0x38 but that does not matter */
	if ((scancode & 0x7f) == 0x38 /* ALT */) {
		if (scancode & 0x80)
			priv->kbd_flags &= ~ATKBD_FLAG_ALT;
		else
			priv->kbd_flags |=  ATKBD_FLAG_ALT;
		return;
	}
	/* right-control is 0xe0 0x1d but that does not matter */
	if ((scancode & 0x7f) == 0x1d /* CONTROL */) {
		if (scancode & 0x80)
			priv->kbd_flags &= ~ATKBD_FLAG_CONTROL;
		else
			priv->kbd_flags |=  ATKBD_FLAG_CONTROL;
		return;
	}
	if (scancode & 0x80) /* release event */
		return;

#ifdef OPTION_KDB
	if ((priv->kbd_flags == (ATKBD_FLAG_CONTROL | ATKBD_FLAG_SHIFT)) && scancode == 1 /* escape */) {
		kdb_enter("keyboard sequence");
		return;
	}
	if (priv->kbd_flags == ATKBD_FLAG_CONTROL && scancode == 0x29 /* tilde */)
		panic("forced by kdb");
#endif

	uint8_t ascii = ((priv->kbd_flags & ATKBD_FLAG_SHIFT) ? atkbd_keymap_shift : atkbd_keymap)[scancode];
	if (ascii == 0)
		return;

	priv->kbd_buffer[(int)priv->kbd_buffer_writepos] = ascii;
	priv->kbd_buffer_writepos = (priv->kbd_buffer_writepos + 1) % ATKBD_BUFFER_SIZE;

	/* XXX signal consumers - this is a hack */
	if (console_tty != NULL && tty_get_inputdev(console_tty) == dev) {
		tty_signal_data();
	}
}

static errorcode_t
atkbd_attach(device_t dev)
{
	void* res_io  = device_alloc_resource(dev, RESTYPE_IO, 7);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	/* Initialize private data; must be done before the interrupt is registered */
	struct ATKBD_PRIVDATA* kbd_priv = kmalloc(sizeof(struct ATKBD_PRIVDATA));
	kbd_priv->kbd_ioport = (uintptr_t)res_io;
	kbd_priv->kbd_buffer_readpos = 0;
	kbd_priv->kbd_buffer_writepos = 0;
	kbd_priv->kbd_flags = 0;
	dev->privdata = kbd_priv;

	if (!irq_register((uintptr_t)res_irq, dev, atkbd_irq)) {
		kfree(kbd_priv);
		return ANANAS_ERROR(NO_RESOURCE);
	}
	
	/*
	 * Ensure the keyboard's input buffer is empty; this will cause it to
	 * send IRQ's to us.
	 */
	inb(kbd_priv->kbd_ioport);

	return ANANAS_ERROR_OK;
}

static errorcode_t
atkbd_read(device_t dev, void* data, size_t* len, off_t off)
{
	struct ATKBD_PRIVDATA* priv = dev->privdata;
	size_t returned = 0, left = *len;

	while (left-- > 0) {
		if (priv->kbd_buffer_readpos == priv->kbd_buffer_writepos)
			break;

		*(uint8_t*)(data + returned++) = priv->kbd_buffer[(int)priv->kbd_buffer_readpos];
		priv->kbd_buffer_readpos = (priv->kbd_buffer_readpos + 1) % ATKBD_BUFFER_SIZE;
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

DEFINE_CONSOLE_DRIVER(drv_atkbd, 10, CONSOLE_FLAG_IN)

/* vim:set ts=2 sw=2: */
