#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/tty.h>
#include <ananas/trace.h>
#include <ananas/x86/io.h>
#include <ananas/dev/kbdmux.h>
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

#define ATKBD_PORT_STATUS 4
#define ATKBD_STATUS_OUTPUT_FULL 1

struct ATKBD_PRIVDATA {
	int	 kbd_ioport;
	uint8_t	kbd_flags;
};

static irqresult_t
atkbd_irq(device_t dev, void* context)
{
	struct ATKBD_PRIVDATA* priv = dev->privdata;

	while(inb(priv->kbd_ioport + ATKBD_PORT_STATUS) & ATKBD_STATUS_OUTPUT_FULL) {
		uint8_t scancode = inb(priv->kbd_ioport);
		if ((scancode & 0x7f) == 0x2a /* LSHIFT */ ||
				(scancode & 0x7f) == 0x36 /* RSHIFT */) {
			if (scancode & 0x80)
				priv->kbd_flags &= ~ATKBD_FLAG_SHIFT;
			else
				priv->kbd_flags |=  ATKBD_FLAG_SHIFT;
			continue;
		}
		/* right-alt is 0xe0 0x38 but that does not matter */
		if ((scancode & 0x7f) == 0x38 /* ALT */) {
			if (scancode & 0x80)
				priv->kbd_flags &= ~ATKBD_FLAG_ALT;
			else
				priv->kbd_flags |=  ATKBD_FLAG_ALT;
			continue;
		}
		/* right-control is 0xe0 0x1d but that does not matter */
		if ((scancode & 0x7f) == 0x1d /* CONTROL */) {
			if (scancode & 0x80)
				priv->kbd_flags &= ~ATKBD_FLAG_CONTROL;
			else
				priv->kbd_flags |=  ATKBD_FLAG_CONTROL;
			continue;
		}
		if (scancode & 0x80) /* release event */
			continue;

#ifdef OPTION_KDB
		if ((priv->kbd_flags == (ATKBD_FLAG_CONTROL | ATKBD_FLAG_SHIFT)) && scancode == 1 /* escape */) {
			kdb_enter("keyboard sequence");
			continue;
		}
		if (priv->kbd_flags == ATKBD_FLAG_CONTROL && scancode == 0x29 /* tilde */)
			panic("forced by kdb");
#endif

		uint8_t ch = ((priv->kbd_flags & ATKBD_FLAG_SHIFT) ? atkbd_keymap_shift : atkbd_keymap)[scancode];
		if (ch != 0)
			kbdmux_on_input(ch);
	}

	return IRQ_RESULT_PROCESSED;
}

static errorcode_t
atkbd_probe(device_t dev)
{
	struct RESOURCE* res = device_get_resource(dev, RESTYPE_PNP_ID, 0);
	if (res != NULL && res->base == 0x0303) /* PNP0303: IBM Enhanced (101/102-key, PS/2 mouse support) */
		return ANANAS_ERROR_OK;
	return ANANAS_ERROR(NO_DEVICE);
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
	kbd_priv->kbd_flags = 0;
	dev->privdata = kbd_priv;

	errorcode_t err = irq_register((uintptr_t)res_irq, dev, atkbd_irq, IRQ_TYPE_DEFAULT, NULL);
	if (err != ANANAS_ERROR_OK) {
		kfree(kbd_priv);
		return err;
	}
	
	/*
	 * Ensure the keyboard's input buffer is empty; this will cause it to
	 * send IRQ's to us.
	 */
	inb(kbd_priv->kbd_ioport);

	return ANANAS_ERROR_OK;
}

struct DRIVER drv_atkbd = {
	.name       = "atkbd",
	.drv_probe  = atkbd_probe,
	.drv_attach = atkbd_attach
};

DRIVER_PROBE(atkbd)
DRIVER_PROBE_BUS(acpi)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
