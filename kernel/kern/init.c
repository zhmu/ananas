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
#define SHELL_BIN "/usr/bin/moonsh.i386"
#elif defined(__amd64__)
#define SHELL_BIN "/usr/bin/moonsh.amd64"
#endif

#define ROOT_DEVICE "slice0"

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
	kprintf("- Mounting / from %s...", ROOT_DEVICE);
	if (vfs_mount(ROOT_DEVICE, "/", "ext2", NULL) == 1) {
		kprintf(" ok\n");
	} else {
		kprintf(" failed\n");
	}

#if 0
	kmem_stats(&mem_avail, &mem_total);
	kprintf("CURRENT Memory: %uKB available / %uKB total\n", mem_avail / 1024, mem_total / 1024);
#endif

	/* Construct our shell process */
#if 1
#ifdef SHELL_BIN 
	thread_t t1 = thread_alloc();

	struct VFS_FILE f;
	kprintf("- Lauching MoonShell from %s...", SHELL_BIN);
	if (vfs_open(SHELL_BIN, &f)) {
		if (elf_load_from_file(t1, &f)) {
			kprintf(" ok\n");
			thread_resume(t1);
		} else {
			kprintf(" fail\n");
		}
	} else {
		kprintf(" fail - file not found\n");
	}
#endif /* SHELL_BIN  */
#endif
#if 0
	thread_t t2 = thread_alloc();

	struct VFS_FILE f;
	kprintf("Lauching q...");
	if (vfs_open("q", &f)) {
		if (elf_load_from_file(t2, &f)) {
			kprintf(" ok\n");
			thread_resume(t2);
		} else {
			kprintf(" fail\n");
		}
	} else {
		kprintf(" fail - file not found\n");
	}
#endif

#ifdef SMP
	smp_launch();
#endif

	/* gooo! */
	scheduler_activate();
	__asm("hlt\n");

	panic("mi_startup(): what now?");
}

/* vim:set ts=2 sw=2: */
