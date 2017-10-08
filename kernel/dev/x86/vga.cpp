#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/console-driver.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "kernel/trace.h"
#include "kernel/x86/io.h"

#include "../../lib/teken/teken.h"

/* Console height */
#define VGA_HEIGHT 25

/* Console width */
#define VGA_WIDTH  80

TRACE_SETUP;

namespace {

struct VGA : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::ICharDeviceOperations
{
	using Device::Device;
	virtual ~VGA() = default;

	IDeviceOperations& GetDeviceOperations() override {
		return *this;
	}

	ICharDeviceOperations* GetCharDeviceOperations() override {
		return this;
	}

	struct Pixel {
		Pixel()
		 : p_c(0), p_a({ 0, 0, 0 })
		{
		}

		Pixel(teken_char_t c, teken_attr_t a)
		 : p_c(c), p_a(a)
		{
		}

		teken_char_t p_c;
		teken_attr_t p_a;
	};

	void WriteCRTC(uint8_t reg, uint8_t val);
	void PutPixel(int x, int y, const Pixel& pixel);
	void SetCursor(int x, int y);

	Pixel& PixelAt(int x, int y)
	{
		return vga_buffer[(y) * VGA_WIDTH + (x)];
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;
	errorcode_t Write(const void* buffer, size_t& len, off_t offset) override;

	uint32_t vga_io;
	uint16_t* vga_video_mem;
	Pixel* vga_buffer;
	uint8_t vga_attr;
	teken_t vga_teken;
	mutex_t vga_mtx_teken;
};

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

static void
term_bell(void* s)
{
	/* nothing */
}

static void
term_cursor(void* s, const teken_pos_t* p)
{
	auto vga = static_cast<VGA*>(s);
	vga->SetCursor(p->tp_col, p->tp_row);
}
	
static void
term_putchar(void *s, const teken_pos_t* p, teken_char_t c, const teken_attr_t* a)
{
	auto vga = static_cast<VGA*>(s);
	auto& pixel = vga->PixelAt(p->tp_col, p->tp_row);
	pixel = VGA::Pixel(c, *a);
	vga->PutPixel(p->tp_col, p->tp_row, pixel);
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
	auto vga = static_cast<VGA*>(s);
	int nrow, ncol, x, y; /* has to be signed - >= 0 comparison */
	teken_pos_t d;

	#define VGA_PLACE_PIXEL \
		vga->PutPixel(d.tp_col, d.tp_row, vga->PixelAt(d.tp_col, d.tp_row))

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
					vga->PixelAt(d.tp_col, d.tp_row) =
					 vga->PixelAt(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					VGA_PLACE_PIXEL;
				}
			}
		} else {
			/* copy from right to left. */
			for (y = 0; y < nrow; y++) {
				d.tp_row = p->tp_row + y;
				for (x = ncol - 1; x >= 0; x--) {
					d.tp_col = p->tp_col + x;
					vga->PixelAt(d.tp_col, d.tp_row) =
					 vga->PixelAt(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
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
					vga->PixelAt(d.tp_col, d.tp_row) =
					 vga->PixelAt(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					VGA_PLACE_PIXEL;
				}
			}
		} else {
			/* copy from right to left. */
			for (y = nrow - 1; y >= 0; y--) {
				d.tp_row = p->tp_row + y;
				for (x = ncol - 1; x >= 0; x--) {
					d.tp_col = p->tp_col + x;
					vga->PixelAt(d.tp_col, d.tp_row) =
					 vga->PixelAt(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
					VGA_PLACE_PIXEL;
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

void
VGA::WriteCRTC(uint8_t reg, uint8_t val)
{
	outb(vga_io + 0x14, reg);
	outb(vga_io + 0x15, val);
}

void
VGA::PutPixel(int x, int y, const Pixel& px)
{
	/* XXX little endian */
	*(uint16_t*)(vga_video_mem + y * VGA_WIDTH + x) =
	 (uint16_t)(teken_256to8(px.p_a.ta_fgcolor) + 8 * teken_256to8(px.p_a.ta_bgcolor)) << 8 |
	 px.p_c;
}

void
VGA::SetCursor(int x, int y)
{
	uint32_t offs = (x + y * VGA_WIDTH);
	WriteCRTC(0xe, offs >> 8);
	WriteCRTC(0xf, offs & 0xff);
}

errorcode_t
VGA::Attach()
{
	void* mem = d_ResourceSet.AllocateResource(Ananas::Resource::RT_Memory, 0x7fff);
	void* res = d_ResourceSet.AllocateResource(Ananas::Resource::RT_IO, 0x1f);
	if (mem == nullptr || res == nullptr)
		return ANANAS_ERROR(NO_RESOURCE);

	vga_io        = (uintptr_t)res;
	vga_video_mem = static_cast<uint16_t*>(mem);
	vga_attr      = 0xf;
	vga_buffer    = new Pixel[VGA_HEIGHT * VGA_WIDTH];

	// Clear the display; we must set the attribute everywhere for the cursor XXX little endian
	uint16_t* ptr = vga_video_mem;
	int size = VGA_HEIGHT * VGA_WIDTH;
	while(size--) {
		*ptr++ = (vga_attr << 8) | ' ';
	}

	// Set cursor shape
	WriteCRTC(0xa, 14);
	WriteCRTC(0xb, 15);

	// Initialize teken library
	teken_pos_t tp;
	tp.tp_row = VGA_HEIGHT; tp.tp_col = VGA_WIDTH;
	teken_init(&vga_teken, &tf, this);
	teken_set_winsize(&vga_teken, &tp);
	mutex_init(&vga_mtx_teken, "vga_mtx_teken");

	return ananas_success();
}

errorcode_t
VGA::Detach()
{
	return ananas_success();
}

errorcode_t
VGA::Write(const void* buffer, size_t& len, off_t offset)
{
	auto ptr = static_cast<const uint8_t*>(buffer);
	size_t left = len;
	mutex_lock(&vga_mtx_teken);
	while(left--) {
		/* XXX this is a hack which should be performed in a TTY layer, once we have one */
		if(*ptr == '\n')
			teken_input(&vga_teken, "\r", 1);
		teken_input(&vga_teken, ptr, 1);
		ptr++;
	}
	mutex_unlock(&vga_mtx_teken);
	return ananas_success();
}

struct VGA_Driver : public Ananas::ConsoleDriver
{
	VGA_Driver()
	 : ConsoleDriver("vga", 100, CONSOLE_FLAG_OUT)
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "corebus";
	}

	Ananas::Device* ProbeDevice() override
	{
		/* XXX Look at the BIOS to see if there is any VGA at all */
		Ananas::ResourceSet resourceSet;
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_Memory, 0xb8000, 128 * 1024));
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_IO, 0x3c0, 32));
		return new VGA(Ananas::CreateDeviceProperties(resourceSet));
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return new VGA(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(VGA_Driver)

/* vim:set ts=2 sw=2: */
