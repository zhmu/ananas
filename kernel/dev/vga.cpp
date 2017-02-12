#include <ananas/types.h>
#include <ananas/error.h>
#include <machine/vm.h>
#include <machine/param.h>
#include <ananas/console.h>
#include <ananas/x86/io.h>
#include <ananas/device.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/thread.h>
#include <ananas/mm.h>

#include <teken.h>

/* If defined, we use a kernel thread to periodically update the screen */
#undef VGA_KERNEL_THREAD

/* Console height */
#define VGA_HEIGHT 25

/* Console width */
#define VGA_WIDTH  80

TRACE_SETUP;

struct pixel {
	teken_char_t	c;
	teken_attr_t	a;
};

struct VGA_PRIVDATA {
	uint32_t vga_io;
	uint16_t* vga_video_mem;
	struct pixel* vga_buffer;
	uint8_t  vga_attr;
	int      vga_dirty;
	teken_t  vga_teken;
	mutex_t	 vga_mtx_teken;
#ifdef VGA_KERNEL_THREAD
	int      vga_cursor_x;
	int      vga_cursor_y;
	semaphore_t vga_sem;
	thread_t vga_workerthread;
#endif
};

#define VGA_PIXEL(s,x,y) ((s)->vga_buffer[(y) * VGA_WIDTH + (x)])

/* XXX this doesn't really belong here */
static tf_bell_t		term_bell;
static tf_cursor_t	term_cursor;
static tf_putchar_t	term_putchar;
static tf_fill_t		term_fill;
static tf_copy_t		term_copy;
static tf_param_t		term_param;
static tf_respond_t	term_respond;

static void vga_crtc_write(struct VGA_PRIVDATA* p, uint8_t reg, uint8_t val);

static teken_funcs_t tf = {
	.tf_bell		= term_bell,
	.tf_cursor	= term_cursor,
	.tf_putchar	= term_putchar,
	.tf_fill		= term_fill,
	.tf_copy		=	term_copy,
	.tf_param		= term_param,
	.tf_respond	= term_respond
};

inline static void
vga_printchar(struct VGA_PRIVDATA* priv, int x, int y, struct pixel* px)
{
	/* XXX little endian */
	*(uint16_t*)(priv->vga_video_mem + y * VGA_WIDTH + x) =
	 (uint16_t)(teken_256to8(px->a.ta_fgcolor) + 8 * teken_256to8(px->a.ta_bgcolor)) << 8 |
	 px->c;
}

inline static void
vga_place_cursor(struct VGA_PRIVDATA* priv, int x, int y)
{
	uint32_t offs = (x + y * VGA_WIDTH);
	vga_crtc_write(priv, 0xe, offs >> 8);
	vga_crtc_write(priv, 0xf, offs & 0xff);
}

#ifdef VGA_KERNEL_THREAD
static void
vga_syncthread(void* ptr)
{
	struct VGA_PRIVDATA* priv = ptr;
	while (1) {
		/* Wait until there is something to do... */
		sem_wait(&priv->vga_sem);

		/* And just place the entire buffer on the screen */
		struct pixel* px = priv->vga_buffer;
		for (unsigned int y = 0; y < VGA_HEIGHT; y++)
			for (unsigned int x = 0; x < VGA_WIDTH; x++, px++)
				vga_printchar(priv, x, y, px);

		/* reposition the cursor */
		vga_place_cursor(priv, priv->vga_cursor_x, priv->vga_cursor_y);
	}
}
#endif

static void
term_bell(void* s)
{
	/* nothing */
}

static void
term_cursor(void* s, const teken_pos_t* p)
{
	auto priv = static_cast<struct VGA_PRIVDATA*>(s);
#ifdef VGA_KERNEL_THREAD
	priv->vga_cursor_x = p->tp_col;
	priv->vga_cursor_y = p->tp_row;
	sem_signal(&priv->vga_sem);
#else
	vga_place_cursor(priv, p->tp_col, p->tp_row);
#endif
}
	
static void
term_putchar(void *s, const teken_pos_t* p, teken_char_t c, const teken_attr_t* a)
{
	auto priv = static_cast<struct VGA_PRIVDATA*>(s);
	VGA_PIXEL(priv, p->tp_col, p->tp_row).c = c;
	VGA_PIXEL(priv, p->tp_col, p->tp_row).a = *a;
#ifdef VGA_KERNEL_THREAD
	sem_signal(&priv->vga_sem);
#else
	vga_printchar(priv, p->tp_col, p->tp_row, &VGA_PIXEL(priv, p->tp_col, p->tp_row));
#endif
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
	auto priv = static_cast<struct VGA_PRIVDATA*>(s);
	int nrow, ncol, x, y; /* has to be signed - >= 0 comparison */
	teken_pos_t d;

#ifdef VGA_KERNEL_THREAD
	#define VGA_PLACE_PIXEL \
		(void)0
#else
	#define VGA_PLACE_PIXEL \
		vga_printchar(priv, d.tp_col, d.tp_row, &VGA_PIXEL(priv, d.tp_col, d.tp_row))
#endif

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
					VGA_PIXEL(priv, d.tp_col, d.tp_row) =
					 VGA_PIXEL(priv, r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					VGA_PLACE_PIXEL;
				}
			}
		} else {
			/* copy from right to left. */
			for (y = 0; y < nrow; y++) {
				d.tp_row = p->tp_row + y;
				for (x = ncol - 1; x >= 0; x--) {
					d.tp_col = p->tp_col + x;
					VGA_PIXEL(priv, d.tp_col, d.tp_row) =
					 VGA_PIXEL(priv, r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					VGA_PLACE_PIXEL;
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
					VGA_PIXEL(priv, d.tp_col, d.tp_row) =
					 VGA_PIXEL(priv, r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					VGA_PLACE_PIXEL;
				}
			}
		} else {
			/* copy from right to left. */
			for (y = nrow - 1; y >= 0; y--) {
				d.tp_row = p->tp_row + y;
				for (x = ncol - 1; x >= 0; x--) {
					d.tp_col = p->tp_col + x;
					VGA_PIXEL(priv, d.tp_col, d.tp_row) =
					 VGA_PIXEL(priv, r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					VGA_PLACE_PIXEL;
				}
			}
		}
	}
#ifdef VGA_KERNEL_THREAD
	sem_signal(&priv->vga_sem);
#endif
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
vga_crtc_write(struct VGA_PRIVDATA* p, uint8_t reg, uint8_t val)
{
	outb(p->vga_io + 0x14, reg);
	outb(p->vga_io + 0x15, val);
}

static errorcode_t
vga_attach(device_t dev)
{
	void* mem = device_alloc_resource(dev, RESTYPE_MEMORY, 0x7fff);
	void* res = device_alloc_resource(dev, RESTYPE_IO, 0x1f);
	if (mem == NULL || res == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	auto vga_privdata = static_cast<struct VGA_PRIVDATA*>(kmalloc(sizeof(struct VGA_PRIVDATA)));
	memset(vga_privdata, 0, sizeof(*vga_privdata));
	vga_privdata->vga_io        = (uintptr_t)res;
	vga_privdata->vga_video_mem = static_cast<uint16_t*>(mem);
	vga_privdata->vga_buffer    = static_cast<struct pixel*>(kmalloc(VGA_HEIGHT * VGA_WIDTH * sizeof(struct pixel)));
	vga_privdata->vga_attr      = 0xf;
	dev->privdata = vga_privdata;
	memset(vga_privdata->vga_buffer, 0, VGA_HEIGHT * VGA_WIDTH * sizeof(struct pixel));

	/* Clear the display; we must set the attribute everywhere for the cursor XXX little endian */
	uint16_t* ptr = vga_privdata->vga_video_mem;
	int size = VGA_HEIGHT * VGA_WIDTH;
	while(size--) {
		*ptr++ = (vga_privdata->vga_attr << 8) | ' ';
	}

	/* set cursor shape */
	vga_crtc_write(vga_privdata, 0xa, 14);
	vga_crtc_write(vga_privdata, 0xb, 15);

	/* initialize teken library */
	teken_pos_t tp;
	tp.tp_row = VGA_HEIGHT; tp.tp_col = VGA_WIDTH;
	teken_init(&vga_privdata->vga_teken, &tf, vga_privdata);
	teken_set_winsize(&vga_privdata->vga_teken, &tp);
	mutex_init(&vga_privdata->vga_mtx_teken, "vga_mtx_teken");

#ifdef VGA_KERNEL_THREAD
	/*
	 * Spawn a thread to synchronize our VGA buffer with the screen; this ensures
	 * we will not busy wait attempting to write the video memory while we could
	 * do a lot more useful things.
	 */
	kthread_init(&vga_privdata->vga_workerthread, "vga", &vga_syncthread, vga_privdata);

	sem_init(&vga_privdata->vga_sem, 0);
	thread_resume(&vga_privdata->vga_workerthread);
#endif
	return ananas_success();
}

static errorcode_t
vga_write(device_t dev, const void* data, size_t* len, off_t off)
{
	auto priv = static_cast<struct VGA_PRIVDATA*>(dev->privdata);
	auto ptr = static_cast<const uint8_t*>(data);
	size_t left = *len;
	mutex_lock(&priv->vga_mtx_teken);
	while(left--) {
		/* XXX this is a hack which should be performed in a TTY layer, once we have one */
		if(*ptr == '\n')
			teken_input(&priv->vga_teken, "\r", 1);
		teken_input(&priv->vga_teken, ptr, 1);
		ptr++;
	}
	mutex_unlock(&priv->vga_mtx_teken);
	return ananas_success();
}

static errorcode_t
vga_probe(device_t dev)
{
	/* XXX Look at the BIOS to see if there is any VGA at all */
	if (!device_add_resource(dev, RESTYPE_MEMORY, 0xb8000, 128 * 1024) ||
	    !device_add_resource(dev, RESTYPE_IO, 0x3c0, 32))
		return ANANAS_ERROR(NO_DEVICE);

	return ananas_success();
}

struct DRIVER drv_vga = {
	.name					= "vga",
	.drv_probe		= vga_probe,
	.drv_attach		= vga_attach,
	.drv_write		= vga_write
};

DRIVER_PROBE(vga)
DRIVER_PROBE_BUS(corebus)
DRIVER_PROBE_END()

DEFINE_CONSOLE_DRIVER(drv_vga, 10, CONSOLE_FLAG_OUT)

/* vim:set ts=2 sw=2: */
