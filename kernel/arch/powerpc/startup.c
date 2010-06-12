#include <sys/types.h>
#include <ofw.h>

ofw_entry_t ofw_entry;

void
md_startup(uint32_t r3, uint32_t r4, uint32_t r5)
{
	ofw_entry = r5;
	ofw_init();

	ofw_putch('!');
}

/* vim:set ts=2 sw=2: */
