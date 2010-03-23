#include <sys/device.h>
#include <sys/bio.h>
#include <sys/console.h>
#include <sys/mm.h>
#include <sys/lib.h>
#include <sys/thread.h>
#include <sys/vfs.h>
#include <machine/vm.h>
#include <elf.h>
#include "options.h"

#ifdef __i386__
#define SHELL_BIN "usr/bin/moonsh.i386"
#elif defined(__amd64__)
#define SHELL_BIN "usr/bin/moonsh.amd64"
#endif

void smp_init();
void smp_launch();

void kmem_dump();

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

	/* Initialize I/O */
	bio_init();

	/* Give the devices a spin */
	device_init();

	/* Init VFS and mount something */
	vfs_init();
	kprintf("vfs_mount(): %i\n", vfs_mount("slice0", "/", "ext2", NULL));

	kmem_stats(&mem_avail, &mem_total);
	kprintf("CURRENT Memory: %uKB available / %uKB total\n", mem_avail / 1024, mem_total / 1024);

	/* Construct our shell process */
#ifdef SHELL_BIN 
	thread_t t1 = thread_alloc();
	struct VFS_FILE f;
	kprintf("Lauching MoonShell.. ");
	if (vfs_open(SHELL_BIN, &f)) {
		if (elf_load_from_file(t1, &f)) {
			kprintf(" ok\n");
		} else {
			kprintf(" fail\n");
		}
	} else {
		kprintf(" fail - file not found\n");
	}
#endif /* SHELL_BIN  */

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
