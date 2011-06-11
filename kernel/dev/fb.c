#include <machine/vm.h>
#include <machine/param.h>
#include <ananas/bootinfo.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <ananas/wii/video.h>
#include <ofw.h>
#include "options.h"

#include <teken.h>

#ifndef WII
/* XXX this is a kludge */
#include <ananas/trace.h>
TRACE_SETUP;
#endif

#include "fb_font.c" /* XXX this is a kludge */

struct pixel {
	teken_char_t	c;
	teken_attr_t	a;
};

struct FB_PRIVDATA {
	void*         fb_memory;
	int           fb_bytes_per_line;
	int           fb_depth;
	int           fb_height; /* note: in chars */
	int           fb_width;  /* note: in chars */
	int           fb_x;
	int           fb_y;
	teken_t       fb_teken;
	struct pixel* fb_buffer;
	int           fb_font_height;
	int           fb_font_width;
	struct FONT*  fb_font;
};

#define FB_BUFFER(x,y) (fb->fb_buffer[(y)*fb->fb_width+(x)])

/* XXX this doesn't really belong here */
static tf_bell_t		term_bell;
static tf_cursor_t	term_cursor;
static tf_putchar_t	term_putchar;
static tf_fill_t		term_fill;
static tf_copy_t		term_copy;
static tf_param_t		term_param;
static tf_respond_t	term_respond;

static teken_funcs_t tf = {
	.tf_bell		= term_bell,
	.tf_cursor	= term_cursor,
	.tf_putchar	= term_putchar,
	.tf_fill		= term_fill,
	.tf_copy		=	term_copy,
	.tf_param		= term_param,
	.tf_respond	= term_respond
};

#if defined(OFW) || defined(__i386__) || defined(__amd64__)
static uint32_t make_rgb(int color)
{
	return !color ? 0x00ff00 : 0;
}

static void putpixel(struct FB_PRIVDATA* fb,unsigned int x, unsigned int y, int color)
{
	uint8_t* ptr = (uint8_t*)(fb->fb_memory + fb->fb_bytes_per_line * y + x * (fb->fb_depth / 8));
	if (fb->fb_depth == 8)
		*ptr = color;
	else if (fb->fb_depth == 16)
		*(uint16_t*)ptr = color;
	else if (fb->fb_depth == 32) {
		uint32_t rgb = make_rgb(color);
		*(uint32_t*)ptr = rgb;
	} else if (fb->fb_depth == 24) {
		uint32_t rgb = make_rgb(color);
		*ptr++ = (rgb >> 16);
		*ptr++ = (rgb >> 8) & 0xff;
		*ptr++ = (rgb & 0xff);
	}
}
#endif

static void
fb_putchar(struct FB_PRIVDATA* fb, unsigned int x, unsigned int y, const unsigned char ch)
{
#if defined(OFW) || defined(__i386__) || defined(__amd64__)
	/* Draw the new char */
	struct CHARACTER* c = &fb->fb_font->chars[ch];
	y += fb->fb_font->height - c->yshift;
	for (int j = 0; j < c->height; j++) {
		for (int i = 0; i <= c->width; i++) {
			uint8_t d = c->data[j];
			if (d & (1 << i))
				putpixel(fb, x + i, y + j, 0);
			else
				putpixel(fb, x + i, y + j, 0xff);
		}
	}
	/* Remove old pixels that aren't part of our new char */
	for (int j = c->height; j < fb->fb_font_height; j++)
		for (int i = c->width; i <= fb->fb_font_width; i++)
			putpixel(fb, x + i, y + j, 0xff);
#elif defined(WII)
	/*
	 * The Wii has a YUV2-encoded framebuffer; this means we'll have to write two
	 * pixels at the same time.
	 */
	uint32_t fg_color = 0xffff00;
	uint32_t bg_color = 0x000000;

	struct CHARACTER* c = &fb->fb_font->chars[ch];
	y += fb->fb_font->height - c->yshift;
	for (int j = 0; j < c->height; j++) {
		uint32_t* dst = (uint32_t*)(fb->fb_memory + (fb->fb_bytes_per_line * (y + j)) + (x * 2));
		for (int i = 0; i <= c->width / 2; i++) {
			uint8_t v1 = c->data[j] & (1 << (i * 2));
			uint8_t v2;
			if (j < 8)
				v2 = c->data[j] & (1 << ((i * 2) + 1));
			else
				v2 = c->data[j + 1] & 1;

			uint32_t color;
			uint32_t rgb1 = v1 ? fg_color : bg_color;
			uint32_t rgb2 = v2 ? fg_color : bg_color;

			/*
			 * Converts RGB to Y'CbCr; based on the Wikipedia article
			 * (http://en.wikipedia.org/wiki/YCbCr; note that floating point is
			 * avoided by multiplying everything) and libogc.
			 */
#define CONV_RGB_TO_YCBCR(y, cb, cr, r, g, b) \
	do { \
    (y) = (299 * (r) + 587 * (g) + 114 * (b)) / 1000; \
    (cb) = (-16874 * (r) - 33126 * (g) + 50000 * (b) + 12800000) / 100000; \
    (cr) = (50000 * (r) - 41869 * (g) - 8131 * (b) + 12800000) / 100000; \
	} while(0)

			int y1, cb1, cr1, y2, cb2, cr2;
			CONV_RGB_TO_YCBCR(y1, cb1, cr1, (rgb1 >> 24) & 0xff, (rgb1 >> 16) & 0xff, (rgb1 & 0xff));
			CONV_RGB_TO_YCBCR(y2, cb2, cr2, (rgb2 >> 24) & 0xff, (rgb2 >> 16) & 0xff, (rgb2 & 0xff));

#undef CONV_RGB_TO_YCBCR

			uint8_t cb = (cb1 + cb2) / 2;
			uint8_t cr = (cr1 + cr2) / 2;

			color = (y1 << 24) | (cb << 16) | (y2 << 8) | cr;
			*dst++ = color;
		}
	}
#endif
}

static void
printchar(struct FB_PRIVDATA* fb, const teken_pos_t* p)
{
	KASSERT(p->tp_row < fb->fb_height, "row not in range");
	KASSERT(p->tp_col < fb->fb_width, "col not in range");
	struct pixel* px = &FB_BUFFER(p->tp_col, p->tp_row);

	if (fb->fb_memory == NULL)
		return;

	fb_putchar(fb, p->tp_col * fb->fb_font_width, p->tp_row * fb->fb_font_height, px->c);
}

static void
term_bell(void* s)
{
	/* nothing */
}

static void
term_cursor(void* s, const teken_pos_t* p)
{
	/* nothing yet */
}
	
static void
term_putchar(void *s, const teken_pos_t* p, teken_char_t c, const teken_attr_t* a)
{
	struct FB_PRIVDATA* fb = (struct FB_PRIVDATA*)s;
	FB_BUFFER(p->tp_col, p->tp_row).c = c;
	FB_BUFFER(p->tp_col, p->tp_row).a = *a;
	printchar(fb, p);
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
	struct FB_PRIVDATA* fb = (struct FB_PRIVDATA*)s;
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
					FB_BUFFER(d.tp_col, d.tp_row) =
					 FB_BUFFER(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					printchar(fb, &d);
				}
			}
		} else {
			/* copy from right to left. */
			for (y = 0; y < nrow; y++) {
				d.tp_row = p->tp_row + y;
				for (x = ncol - 1; x >= 0; x--) {
					d.tp_col = p->tp_col + x;
					FB_BUFFER(d.tp_col, d.tp_row) =
					 FB_BUFFER(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					printchar(fb, &d);
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
					FB_BUFFER(d.tp_col, d.tp_row) =
					 FB_BUFFER(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					printchar(fb, &d);
				}
			}
		} else {
			/* copy from right to left. */
			for (y = nrow - 1; y >= 0; y--) {
				d.tp_row = p->tp_row + y;
				for (x = ncol - 1; x >= 0; x--) {
					d.tp_col = p->tp_col + x;
					FB_BUFFER(d.tp_col, d.tp_row) =
					 FB_BUFFER(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					printchar(fb, &d);
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

static errorcode_t
fb_write(device_t dev, const void* data, size_t* len, off_t offset)
{
	struct FB_PRIVDATA* fb = (struct FB_PRIVDATA*)dev->privdata;
	const uint8_t* ptr = data;
	size_t left = *len;
	while(left--) {
		/* XXX this is a hack which should be performed in a TTY layer, once we have one */
		if(*ptr == '\n')
			teken_input(&fb->fb_teken, "\r", 1);
		teken_input(&fb->fb_teken, ptr, 1);
		ptr++;
	}
	return ANANAS_ERROR_OK;
}

static errorcode_t
fb_probe(device_t dev)
{
#ifdef OFW
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_cell_t ihandle_stdout;
	ofw_getprop(chosen, "stdout", &ihandle_stdout, sizeof(ihandle_stdout));

	ofw_cell_t node = ofw_instance_to_package(ihandle_stdout);
	if (node == -1)
		return ANANAS_ERROR(NO_DEVICE);

	char type[16] = {0};
	ofw_getprop(node, "device_type", type, sizeof(type));
	if (strcmp(type, "display") != 0) {
		/* Not a framebuffer-backed device; bail out */
		return ANANAS_ERROR(NO_DEVICE);
	}
#elif defined(WII)
	return wiivideo_init();
#elif defined(__i386__) || defined(__amd64__)
	if (bootinfo->bi_video_xres == 0 ||
	    bootinfo->bi_video_yres == 0 ||
	    bootinfo->bi_video_bpp == 0 ||
	    bootinfo->bi_video_framebuffer == 0) {
		/* Loader didn't supply a video device; bail */
		return ANANAS_ERROR(NO_DEVICE);
	}
#endif
	return ANANAS_ERROR_OK; 
}

static errorcode_t
fb_attach(device_t dev)
{
	int height, width, depth, bytes_per_line;
#ifdef OFW
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_cell_t ihandle_stdout;
	ofw_getprop(chosen, "stdout", &ihandle_stdout, sizeof(ihandle_stdout));

	ofw_cell_t node = ofw_instance_to_package(ihandle_stdout);
	if (node == -1)
		return ANANAS_ERROR(NO_DEVICE);

	int physaddr;
	ofw_getprop(node, "height", &height, sizeof(height));
	ofw_getprop(node, "width", &width, sizeof(width));
	ofw_getprop(node, "depth", &depth, sizeof(depth));
	ofw_getprop(node, "linebytes", &bytes_per_line, sizeof(bytes_per_line));
	ofw_getprop(node, "address", &physaddr, sizeof(physaddr));

	/* Map the video buffer and clear it */
	void* phys = vm_map_kernel(physaddr, ((height * bytes_per_line) + PAGE_SIZE - 1) / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
	memset((void*)phys, 0xff, (height * bytes_per_line));
#elif defined(WII)
	void* phys = wiivideo_get_framebuffer();
	wiivideo_get_size(&height, &width);
	depth = 16;
	bytes_per_line = width * 2;
#elif defined(__i386__) || defined(__amd64__)
	width = bootinfo->bi_video_xres;
	height = bootinfo->bi_video_yres;
	depth = bootinfo->bi_video_bpp;
	bytes_per_line = width * (depth / 8);
	void* phys = vm_map_kernel(bootinfo->bi_video_framebuffer, ((height * bytes_per_line) + PAGE_SIZE - 1) / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
#endif

	struct FB_PRIVDATA* fb = (struct FB_PRIVDATA*)kmalloc(sizeof(struct FB_PRIVDATA));
	fb->fb_memory = (void*)phys;
	fb->fb_font_height = 12; /* XXX */
	fb->fb_font_width = 10; /* XXX */
	fb->fb_font = &font12; /* XXX */
	fb->fb_bytes_per_line = bytes_per_line;
	fb->fb_depth = depth;
	/* XXX Height is one less because of the way our glyphs are drawn (bottom up) */
	fb->fb_height = (height / fb->fb_font_height) - 1;
	fb->fb_width = width / fb->fb_font_width;
	fb->fb_x = 0;
	fb->fb_y = 0;
	fb->fb_buffer = kmalloc(fb->fb_height * fb->fb_width * sizeof(struct pixel));
	memset(fb->fb_buffer, 0, fb->fb_height * fb->fb_width * sizeof(struct pixel));
	dev->privdata = fb;

	/* initialize teken library */
	teken_pos_t tp;
	tp.tp_row = fb->fb_height; tp.tp_col = fb->fb_width;
	teken_init(&fb->fb_teken, &tf, fb);
	teken_set_winsize(&fb->fb_teken, &tp);

	kprintf("%s: console is %u x %u @ %u bpp\n",
	 dev->name, fb->fb_width, fb->fb_height, fb->fb_depth);
   
	return ANANAS_ERROR_OK;
}

struct DRIVER drv_fb = {
	.name					= "fb",
	.drv_probe		= fb_probe,
	.drv_attach		= fb_attach,
	.drv_write		= fb_write
};

DRIVER_PROBE(fb)
DRIVER_PROBE_BUS(corebus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
