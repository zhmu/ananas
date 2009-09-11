#include "i386/vm.h"
#include "i386/io.h"
#include "console.h"
#include "device.h"
#include "lib.h"
#include "vm.h"

/* Base memory address of the console memory */
#define VGA_MEM_BASE 0xb8000

/* Video memory length */
#define VGA_MEM_LENGTH 4000

/* Console height */
#define VGA_HEIGHT 25

/* Console width */
#define VGA_WIDTH  80

static uint8_t* vga_mem = NULL;
static uint8_t vga_x = 0;
static uint8_t vga_y = 0;
static uint8_t vga_attr = 0xf;

static int
vga_attach(device_t dev)
{
	vga_mem = (uint8_t*)vm_map_device(VGA_MEM_BASE, VGA_MEM_LENGTH);
	memset(vga_mem, 0, VGA_MEM_LENGTH);

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
			if (vga_y >= VGA_HEIGHT) {
				/* Scroll the screen up a row */
				memcpy((void*)(vga_mem),
							 (void*)(vga_mem + (VGA_WIDTH * 2)),
							 (VGA_HEIGHT - 1) * VGA_WIDTH * 2);
				vga_y--;
			}
		}
	}

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
