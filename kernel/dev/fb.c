#include <machine/vm.h>
#include <machine/param.h>
#include <sys/console.h>
#include <sys/device.h>
#include <sys/lib.h>
#include <sys/mm.h>
#include <ofw.h>

#include <teken.h>

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

#define FB_BUFFER(x,y) (fb->fb_buffer[(y)*fb->fb_height+(x)])

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

static void putpixel(struct FB_PRIVDATA* fb,unsigned int x, unsigned int y, int color)
{
	*(uint8_t*)(fb->fb_memory + fb->fb_bytes_per_line * y + x) = color;
}

static void
fb_putchar(struct FB_PRIVDATA* fb, unsigned int x, unsigned int y, const unsigned char ch)
{
	struct CHARACTER* c = &fb->fb_font->chars[ch];
	y += fb->fb_font->height;
	for (int j = 0; j < c->height; j++) {
		for (int i = 0; i <= c->width; i++) {
			uint8_t d = c->data[j];
			if (d & (1 << i))
				putpixel(fb, x + i, y + j - c->yshift, 0);
		}
	}
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

static ssize_t
fb_write(device_t dev, const char* data, size_t len)
{
	struct FB_PRIVDATA* fb = (struct FB_PRIVDATA*)dev->privdata;
	size_t origlen = len;
	while(len--) {
		/* XXX this is a hack which should be performed in a TTY layer, once we have one */
		if(*data == '\n')
			teken_input(&fb->fb_teken, "\r", 1);
		teken_input(&fb->fb_teken, data, 1);
		data++;
	}
	return origlen;
}


static int
fb_attach(device_t dev)
{
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_cell_t ihandle_stdout;
	ofw_getprop(chosen, "stdout", &ihandle_stdout, sizeof(ihandle_stdout));
	
	kprintf("fb_attach!\n");

	ofw_cell_t node = ofw_instance_to_package(ihandle_stdout);
	if (node == -1)
		return 1;

	char type[16] = {0};
	int height, width, depth, bytes_per_line, phys;
	ofw_getprop(node, "device_type", type, sizeof(type));
	ofw_getprop(node, "height", &height, sizeof(height));
	ofw_getprop(node, "width", &width, sizeof(width));
	ofw_getprop(node, "depth", &depth, sizeof(depth));
	ofw_getprop(node, "linebytes", &bytes_per_line, sizeof(bytes_per_line));
	ofw_getprop(node, "address", &phys, sizeof(phys));
	if (strcmp(type, "display") != 0) {
		/* Not a framebuffer-backed device; bail out */
		return 1;
	}

	struct FB_PRIVDATA* fb = (struct FB_PRIVDATA*)kmalloc(sizeof(struct FB_PRIVDATA));
	fb->fb_memory = (void*)phys;
	fb->fb_font_height = 12; /* XXX */
	fb->fb_font_width = 10; /* XXX */
	fb->fb_font = &font12; /* XXX */
	fb->fb_bytes_per_line = bytes_per_line;
	fb->fb_depth = depth;
	fb->fb_height = height / fb->fb_font_height;
	fb->fb_width = width / fb->fb_font_width;
	fb->fb_x = 0;
	fb->fb_y = 0;
	fb->fb_buffer = kmalloc(fb->fb_height * fb->fb_width * sizeof(struct pixel));
	memset(fb->fb_buffer, 0, fb->fb_height * fb->fb_width * sizeof(struct pixel));
	dev->privdata = fb;

	/* map the framebuffer nd clear it */	
	vm_map(phys, ((height * bytes_per_line) + PAGE_SIZE - 1) / PAGE_SIZE);
	memset((void*)phys, 0xff, (height * bytes_per_line));

	/* initialize teken library */
	teken_pos_t tp;
	tp.tp_row = fb->fb_height; tp.tp_col = fb->fb_width;
	teken_init(&fb->fb_teken, &tf, fb);
	teken_set_winsize(&fb->fb_teken, &tp);

kprintf("teken initted; console is %u x %u @ %u bpp\n",
 fb->fb_height, fb->fb_width, fb->fb_depth);
	return 0;
}

struct DRIVER drv_fb = {
	.name					= "fb",
	.drv_probe		= NULL,
	.drv_attach		= fb_attach,
	.drv_write		= fb_write
};

DRIVER_PROBE(fb)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
