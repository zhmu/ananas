#include <ananas/types.h>
#include <ananas/error.h>
#include <machine/vm.h>
#include <ananas/console.h>
#include <ananas/x86/io.h>
#include <ananas/device.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/mm.h>

#include <teken.h>

/* Console height */
#define VGA_HEIGHT 25

/* Console width */
#define VGA_WIDTH  80

TRACE_SETUP;

static uint32_t vga_io = 0;
static uint8_t* vga_mem = NULL;
static uint8_t vga_attr = 0xf;
static teken_t vga_teken;

/* XXX this doesn't really belong here */
static tf_bell_t		term_bell;
static tf_cursor_t	term_cursor;
static tf_putchar_t	term_putchar;
static tf_fill_t		term_fill;
static tf_copy_t		term_copy;
static tf_param_t		term_param;
static tf_respond_t	term_respond;

static void vga_crtc_write(device_t dev, uint8_t reg, uint8_t val);

static teken_funcs_t tf = {
	.tf_bell		= term_bell,
	.tf_cursor	= term_cursor,
	.tf_putchar	= term_putchar,
	.tf_fill		= term_fill,
	.tf_copy		=	term_copy,
	.tf_param		= term_param,
	.tf_respond	= term_respond
};

struct pixel {
	teken_char_t	c;
	teken_attr_t	a;
};

static struct pixel buffer[VGA_WIDTH][VGA_HEIGHT];

static void
printchar(const teken_pos_t* p)
{
	KASSERT(p->tp_row < VGA_HEIGHT, "row not in range");
	KASSERT(p->tp_col < VGA_WIDTH, "col not in range");
	struct pixel* px = &buffer[p->tp_col][p->tp_row];

	if (vga_mem == NULL)
		return;

	addr_t offs = (p->tp_row * VGA_WIDTH + p->tp_col) * 2;
	*(vga_mem + offs    ) = px->c;
	*(vga_mem + offs + 1) = teken_256to8(px->a.ta_fgcolor) + 8 * teken_256to8(px->a.ta_bgcolor);
}

static void
term_bell(void* s)
{
	/* nothing */
}

static void
term_cursor(void* s, const teken_pos_t* p)
{
	/* reposition the cursor */
	int offs = (p->tp_col + p->tp_row * VGA_WIDTH);
	vga_crtc_write(s, 0xe, offs >> 8);
	vga_crtc_write(s, 0xf, offs & 0xff);
}
	
static void
term_putchar(void *s, const teken_pos_t* p, teken_char_t c, const teken_attr_t* a)
{
	buffer[p->tp_col][p->tp_row].c = c;
	buffer[p->tp_col][p->tp_row].a = *a;
	printchar(p);
}

static void
term_fill(void* s, const teken_rect_t* r, teken_char_t c, const teken_attr_t* a)
{
	teken_pos_t p;

	/* Braindead implementation of fill() - just call putchar(). */
	for (p.tp_row = r->tr_begin.tp_row; p.tp_row < r->tr_end.tp_row; p.tp_row++)
		for (p.tp_col = r->tr_begin.tp_col; p.tp_col < r->tr_end.tp_col; p.tp_col++)
			term_putchar(s, &p, c, a);
}

static void
term_copy(void* s, const teken_rect_t* r, const teken_pos_t* p)
{
       int nrow, ncol, x, y; /* has to be signed - >= 0 comparison */
        teken_pos_t d;

        /*
         * copying is a little tricky. we must make sure we do it in
         * correct order, to make sure we don't overwrite our own data.
         */

        nrow = r->tr_end.tp_row - r->tr_begin.tp_row;
        ncol = r->tr_end.tp_col - r->tr_begin.tp_col;

        if (p->tp_row < r->tr_begin.tp_row) {
                /* copy from top to bottom. */
                if (p->tp_col < r->tr_begin.tp_col) {
                        /* copy from left to right. */
                        for (y = 0; y < nrow; y++) {
                                d.tp_row = p->tp_row + y;
                                for (x = 0; x < ncol; x++) {
                                        d.tp_col = p->tp_col + x;
                                        buffer[d.tp_col][d.tp_row] =
                                            buffer[r->tr_begin.tp_col + x][r->tr_begin.tp_row + y];
                                        printchar(&d);
                                }
                        }
                } else {
                        /* copy from right to left. */
                        for (y = 0; y < nrow; y++) {
                                d.tp_row = p->tp_row + y;
                                for (x = ncol - 1; x >= 0; x--) {
                                        d.tp_col = p->tp_col + x;
                                        buffer[d.tp_col][d.tp_row] =
                                            buffer[r->tr_begin.tp_col + x][r->tr_begin.tp_row + y];
                                        printchar(&d);
                                }
                        }
                }
        } else {
                /* copy from bottom to top. */
                if (p->tp_col < r->tr_begin.tp_col) {
                        /* copy from left to right. */
                        for (y = nrow - 1; y >= 0; y--) {
                                d.tp_row = p->tp_row + y;
                                for (x = 0; x < ncol; x++) {
                                        d.tp_col = p->tp_col + x;
                                        buffer[d.tp_col][d.tp_row] =
                                            buffer[r->tr_begin.tp_col + x][r->tr_begin.tp_row + y];
                                        printchar(&d);
                                }
                        }
                } else {
                        /* copy from right to left. */
                        for (y = nrow - 1; y >= 0; y--) {
                                d.tp_row = p->tp_row + y;
                                for (x = ncol - 1; x >= 0; x--) {
                                        d.tp_col = p->tp_col + x;
                                        buffer[d.tp_col][d.tp_row] =
                                            buffer[r->tr_begin.tp_col + x][r->tr_begin.tp_row + y];
                                        printchar(&d);
                                }
                        }
                }
        }
}

static void
term_param(void* s, int cmd, unsigned int value)
{
	/* TODO */
}

static void
term_respond(void* s, const void* buf, size_t len)
{
	/* TODO */
}

static void
vga_crtc_write(device_t dev, uint8_t reg, uint8_t val)
{
	outb(vga_io + 0x14, reg);
	outb(vga_io + 0x15, val);
}

static errorcode_t
vga_attach(device_t dev)
{
	vga_mem = device_alloc_resource(dev, RESTYPE_MEMORY, 0x7fff);
	void* res = device_alloc_resource(dev, RESTYPE_IO, 0x1f);
	if (vga_mem == NULL || res == NULL)
		return ANANAS_ERROR(NO_RESOURCE);
	vga_io = (uintptr_t)res;

	/* Clear the display; we must set the attribute everywhere for the cursor */
	uint8_t* ptr = vga_mem;
	int size = VGA_HEIGHT * VGA_WIDTH;
	while(size--) {
		*ptr++ = ' '; *ptr++ = vga_attr;
	}

	/* set cursor shape */
	vga_crtc_write(dev, 0xa, 14);
	vga_crtc_write(dev, 0xb, 15);

	/* initialize teken library */
	teken_pos_t tp;
	tp.tp_row = VGA_HEIGHT; tp.tp_col = VGA_WIDTH;
	teken_init(&vga_teken, &tf, NULL);
	teken_set_winsize(&vga_teken, &tp);
	return ANANAS_ERROR_OK;
}

static errorcode_t
vga_write(device_t dev, const void* data, size_t* len, off_t off)
{
	const uint8_t* ptr = data;
	size_t left = *len;
	while(left--) {
		/* XXX this is a hack which should be performed in a TTY layer, once we have one */
		if(*ptr == '\n')
			teken_input(&vga_teken, "\r", 1);
		teken_input(&vga_teken, ptr, 1);
		ptr++;
	}
	return ANANAS_ERROR_OK;
}

struct DRIVER drv_vga = {
	.name					= "vga",
	.drv_probe		= NULL,
	.drv_attach		= vga_attach,
	.drv_write		= vga_write
};

DRIVER_PROBE(vga)
DRIVER_PROBE_BUS(isa)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
