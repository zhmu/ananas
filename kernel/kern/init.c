#include "options.h"
#include "device.h"
#include "console.h"
#include "mm.h"
#include "lib.h"
#include "thread.h"
#include "machine/vm.h"

#include "../../../../sh.inc"
#include "../../../../q.inc"

int elf32_load(thread_t t, const char* data, size_t datalen);

void smp_init();
void smp_launch();

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
	kprintf("Hello world, this is Ananas/%s %u.%u\n", ARCHITECTURE, 0, 1);
	kprintf("Memory: %uKB available / %uKB total\n", mem_avail / 1024, mem_total / 1024);

#ifdef SMP
	/* Try the SMP dance */
	smp_init();
#endif

	/* Give the devices a spin */
	device_init();

#if 0
	thread_t t1 = thread_alloc();
	elf32_load(t1, sh, sh_LENGTH);
	thread_t t2 = thread_alloc();
	elf32_load(t2, q, q_LENGTH);
	thread_t t3 = thread_alloc();
	elf32_load(t3, sh, q_LENGTH);
#endif

#ifdef SMP
//	smp_launch();
#endif

	while(1);

#if 1
	/* gooo! */
	__asm(
		"sti\n"
		"hlt\n"
	);
#endif

	panic("mi_startup(): what now?");
}

/* vim:set ts=2 sw=2: */
