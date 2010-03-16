#include <sys/device.h>
#include <sys/console.h>
#include <sys/mm.h>
#include <sys/lib.h>
#include <sys/thread.h>
#include <machine/vm.h>
#include "options.h"

#ifdef __i386__
#include "../../../../../output/moonsh.i386.inc"
#endif

#ifdef __amd64__
#include "../../../../../output/moonsh.amd64.inc"
#endif

int elf32_load(thread_t t, const char* data, size_t datalen);
int elf64_load(thread_t t, const char* data, size_t datalen);

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

/*
	__asm(
		"movl	$0x12345678, %eax\n"
		"movl	$0x87654321, %ebx\n"
		"movl	$0xf00dbabe, %ecx\n"
		"movl	$0xdeadd00d, %edx\n"
		"movl	$0x1, %esi\n"
		"movl	$0x2, %edi\n"
		"movl	$0x3, %ebp\n"
		"movl (0), %eax\n"
	);
*/

#ifdef SMP
	/* Try the SMP dance */
	smp_init();
#endif

	/* Give the devices a spin */
	device_init();

	/* Construct our shell process */
	thread_t t1 = thread_alloc();
#ifdef __amd64__
	elf64_load(t1, (char*)moonsh, moonsh_LENGTH);
#else
	elf32_load(t1, (char*)moonsh, moonsh_LENGTH);
#endif

#ifdef SMP
	smp_launch();
#endif

	/* gooo! */
	__asm(
		"sti\n"
		"hlt\n"
	);

	panic("mi_startup(): what now?");
}

/* vim:set ts=2 sw=2: */
