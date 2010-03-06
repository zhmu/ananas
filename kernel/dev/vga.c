#include "machine/vm.h"
#include "machine/io.h"
#include "console.h"
#include "device.h"
#include "lib.h"
#include "vm.h"

/* Console height */
#define VGA_HEIGHT 25

/* Console width */
#define VGA_WIDTH  80

static uint32_t vga_io = 0;
static uint8_t* vga_mem = NULL;
static uint8_t vga_x = 0;
static uint8_t vga_y = 0;
static uint8_t vga_attr = 0xf;

static void
vga_crtc_write(device_t dev, uint8_t reg, uint8_t val)
{
	outb(vga_io + 0x14, reg);
	outb(vga_io + 0x15, val);
}

static int
vga_attach(device_t dev)
{
	vga_mem = device_alloc_resource(dev, RESTYPE_MEMORY, 0x7fff);
	void* res = device_alloc_resource(dev, RESTYPE_IO, 0x1f);
	if (vga_mem == NULL || res == NULL)
		return 1; /* XXX */
	vga_io = (uint32_t)res;

	/* Clear the display; we must set the attribute everywhere for the cursor */
	uint8_t* ptr = vga_mem;
	int size = VGA_HEIGHT * VGA_WIDTH;
	while(size--) {
		*ptr++ = ' '; *ptr++ = vga_attr;
	}

	/* set cursor shape */
	vga_crtc_write(dev, 0xa, 14);
	vga_crtc_write(dev, 0xb, 15);
	return 0;
}

static ssize_t
vga_write(device_t dev, const char* data, size_t len)
{
	size_t amount;

	if (vga_mem == NULL)
		return -1;

	for (amount = 0; amount < len; amount++, data++) {
		addr_t offs = (vga_y * VGA_WIDTH + vga_x) * 2;
		switch(*data) {
			case '\n':
				vga_x = 0; vga_y++;
				break;
			default:
				*(vga_mem + offs    ) = *data;
				*(vga_mem + offs + 1) = vga_attr;
				vga_x++;
				break;
		}
		if (vga_x >= VGA_WIDTH) {
			vga_x = 0;
			vga_y++;
		}
		if (vga_y >= VGA_HEIGHT) {
			/* Scroll the screen up a row */
			memcpy((void*)(vga_mem),
						 (void*)(vga_mem + (VGA_WIDTH * 2)),
						 (VGA_HEIGHT - 1) * VGA_WIDTH * 2);
			/* Blank the bottom row */
			memset((void*)(vga_mem + (VGA_WIDTH * 2) * (VGA_HEIGHT - 1)), 0, (VGA_WIDTH * 2));
			vga_y--;
		}
	}

	/* reposition the cursor */
	vga_crtc_write(dev, 0xe, (vga_x + vga_y * VGA_WIDTH) >> 8);
	vga_crtc_write(dev, 0xf, (vga_x + vga_y * VGA_WIDTH) & 0xff);

	return len;
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
