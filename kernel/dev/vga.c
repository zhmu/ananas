#include "i386/vm.h"
#include "i386/io.h"
#include "console.h"
#include "lib.h"

static uint8_t* vga_mem = NULL;
static uint8_t vga_x = 0;
static uint8_t vga_y = 0;
static uint8_t vga_attr = 0xf;

void
vga_init()
{
	vga_mem = (uint8_t*)vm_map_device(CONSOLE_MEM_BASE, CONSOLE_MEM_LENGTH);
	memset(vga_mem, 0, CONSOLE_MEM_LENGTH);
}

void
vga_putc(char c)
{
	addr_t offs = (vga_y * CONSOLE_WIDTH + vga_x) * 2;

	if (vga_mem == NULL)
		return;

	switch(c) {
		case '\n':
			vga_x = 0; vga_y++;
			break;
		default:
			*(vga_mem + offs    ) = c;
			*(vga_mem + offs + 1) = vga_attr;
			vga_x++;
			break;
	}
	if (vga_x >= CONSOLE_WIDTH) {
		vga_x = 0;
		vga_y++;
		if (vga_y >= CONSOLE_HEIGHT) {
			/* Scroll the screen up a row */
			memcpy((void*)(vga_mem),
			       (void*)(vga_mem + (CONSOLE_WIDTH * 2)),
			       (CONSOLE_HEIGHT - 1) * CONSOLE_WIDTH * 2);
			vga_y--;
		}
	}
}

/* vim:set ts=2 sw=2: */
