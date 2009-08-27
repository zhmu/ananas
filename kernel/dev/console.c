#include "i386/vm.h"
#include "console.h"
#include "lib.h"

static uint8_t* console_mem;
static uint8_t console_x = 0;
static uint8_t console_y = 0;
static uint8_t console_attr = 0xf;

void
console_init()
{
	console_mem = vm_map_device(CONSOLE_MEM_BASE, CONSOLE_MEM_LENGTH);
	memset(console_mem, 0, CONSOLE_MEM_LENGTH);
}

void
console_putc(char c)
{
	addr_t offs = (console_y * CONSOLE_WIDTH + console_x) * 2;

	switch(c) {
		case '\n':
			console_x = 0; console_y++;
			break;
		default:
			*(console_mem + offs    ) = c;
			*(console_mem + offs + 1) = console_attr;
			console_x++;
			break;
	}
	if (console_x >= CONSOLE_WIDTH) {
		console_x = 0;
		console_y++;
		if (console_y >= CONSOLE_HEIGHT) {
			/* Scroll the screen up a row */
			memcpy((console_mem),
			       (console_mem + (CONSOLE_WIDTH * 2)),
			       (CONSOLE_HEIGHT - 1) * CONSOLE_WIDTH * 2);
			console_y--;
		}
	}
}

/* vim:set ts=2 sw=2: */
