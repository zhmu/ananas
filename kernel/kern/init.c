#include <ananas/device.h>
#include <ananas/bio.h>
#include <ananas/console.h>
#include <ananas/error.h>
#include <ananas/mm.h>
#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/schedule.h>
#include <ananas/thread.h>
#include <ananas/tty.h>
#include <ananas/vfs.h>
#include <ananas/waitqueue.h>
#include <machine/vm.h>
#include <elf.h>
#include "options.h"

#define SHELL_BIN "/bin/sh"
#define ROOT_DEVICE "slice0"
#define ROOT_FS_TYPE "fatfs"

void smp_init();
void smp_launch();

void kmem_dump();

void
mi_startup()
{
	errorcode_t err;

	/*
	 * Cheat and initialize the console driver first; this ensures the user
	 * will be able to see the initialization messages ;-)
	 */
	tty_preinit();
	console_init();

	/* Show a startup banner */
	size_t mem_avail, mem_total;
	kmem_stats(&mem_avail, &mem_total);
	kprintf("Ananas/%s - %s %s\n", ARCHITECTURE, __DATE__, __TIME__);
	kprintf("Memory: %uKB available / %uKB total\n", mem_avail / 1024, mem_total / 1024);
#if defined(__i386__) || defined(__amd64__)
	extern int md_cpu_clock_mhz;
	kprintf("CPU: %u MHz\n", md_cpu_clock_mhz);
#endif

#ifdef KDB
	kdb_init();
#endif

#ifdef SMP
	/* Try the SMP dance */
	smp_init();
#endif

	/* Initialize waitqueues */
	waitqueue_init(NULL);

	/* Initialize TTY infastructure */
	tty_init();

	/* Initialize I/O */
	bio_init();

#ifdef USB
	/* Initialize USB stack */
	void usb_init();
	usb_init();
#endif

	/* Give the devices a spin */
	device_init();

	/* Init VFS and mount something */
	vfs_init();
	kprintf("- Mounting / from %s...", ROOT_DEVICE);
	err = vfs_mount(ROOT_DEVICE, "/", ROOT_FS_TYPE, NULL);
	if (err == ANANAS_ERROR_NONE) {
		kprintf(" ok\n");
	} else {
		kprintf(" failed, error %i\n", err);
	}

#if 0
	kmem_stats(&mem_avail, &mem_total);
	kprintf("CURRENT Memory: %uKB available / %uKB total\n", mem_avail / 1024, mem_total / 1024);
#endif

#ifdef SHELL_BIN 
	thread_t t1;
	err = thread_alloc(NULL, &t1);
	if (err == ANANAS_ERROR_NONE) {
		struct VFS_FILE f;
		kprintf("- Lauching %s...", SHELL_BIN);
		err = vfs_open(SHELL_BIN, NULL, &f);
		if (err == ANANAS_ERROR_NONE) {
			err = elf_load_from_file(t1, f.f_inode);
			if (err == ANANAS_ERROR_NONE) {
				kprintf(" ok\n");
				thread_set_args(t1, "sh\0\0", PAGE_SIZE);
				thread_set_environment(t1, "OS=Ananas\0USER=root\0\0", PAGE_SIZE);
				thread_resume(t1);
			} else {
				kprintf(" fail - error %i\n", err);
			}
			vfs_close(&f);
		} else {
			kprintf(" fail - error %i\n", err);
		}
	} else {
		kprintf(" couldn't create process, %i\n", err);
	}
#endif /* SHELL_BIN  */

#ifdef SMP
	smp_launch();
#endif

	/* gooo! */
	scheduler_activate();

	/* Wait for an interrupt to come and steal our context... */
	while (1) {
#if defined(__i386__) || defined(__amd64__)
		__asm("hlt\n");
#endif
	}

	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
