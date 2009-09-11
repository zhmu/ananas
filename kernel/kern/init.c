#include "device.h"
#include "console.h"
#include "mm.h"
#include "lib.h"

void
mi_startup()
{
	size_t mem_avail, mem_total;

	/*
	 * Cheat and initialize the console driver first; this ensures the user
	 * will be able to see the initialization messages ;-)
	 */
	console_init();

	/* Show a startup banner */
	kmem_stats(&mem_avail, &mem_total);
	kprintf("Hello world, this is Ananas/%s %u.%u\n", "i386", 0, 1);
	kprintf("Memory: %uKB available / %uKB total\n", mem_avail / 1024, mem_total / 1024);

	/* Give the devices a spin */
	device_init();

	panic("mi_startup(): what now?");
}

/* vim:set ts=2 sw=2: */
