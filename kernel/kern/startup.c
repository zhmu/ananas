#include "dev/console.h"
#include "lib.h"

void
mi_startup()
{
	vga_init();

	size_t mem_avail, mem_total;
	kmem_stats(&mem_avail, &mem_total);
	kprintf("Hello world, this is Ananas/%s %x.%x\n", "i386", 0, 1);
	kprintf("Memory available: 0x%x / 0x%x bytes\n", mem_avail, mem_total);

	while (1);
}
