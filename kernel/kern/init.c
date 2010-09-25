#include <ananas/device.h>
#include <ananas/bio.h>
#include <ananas/console.h>
#include <ananas/mm.h>
#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/schedule.h>
#include <ananas/thread.h>
#include <ananas/vfs.h>
#include <machine/vm.h>
#include <elf.h>
#include "options.h"

#define SHELL_BIN "/bin/sh"

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

#ifdef KDB
	kdb_init();
#endif

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
	thread_t t1 = thread_alloc(NULL);

	struct VFS_FILE f;
	kprintf("- Lauching %s...", SHELL_BIN);
	if (vfs_open(SHELL_BIN, NULL, &f)) {
		if (elf_load_from_file(t1, &f)) {
			kprintf(" ok\n");
			thread_set_args(t1, "sh\0\0");
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

#if defined(__i386__) || defined(__amd64__)
	__asm("hlt\n");
#else
	/* Wait for an interrupt to come and steal our context... */
	while(1);
#endif

	panic("mi_startup(): what now?");
}

/* vim:set ts=2 sw=2: */
