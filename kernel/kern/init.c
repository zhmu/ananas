#include <ananas/device.h>
#include <ananas/console.h>
#include <ananas/error.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <ananas/init.h>
#include <ananas/module.h>
#include <ananas/symbols.h>
#include <ananas/thread.h>
#include <ananas/tty.h>
#include <ananas/vfs.h>
#include <machine/vm.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include <elf.h>
#include "options.h"

#define SHELL_BIN "/bin/sh"
#define ROOT_DEVICE "slice0"
#define ROOT_FS_TYPE "fatfs"
#define DEVFS_MOUNTPOINT "/dev"
#undef RAMDISK

/* If set, display the entire init tree before launching it */
#undef VERBOSE_INIT

extern void* __initfuncs_begin;
extern void* __initfuncs_end;

static void
run_init()
{
	/*
	 * Create a shadow copy of the init function chain; this is done so that we
	 * can sort it.
	 */
	size_t init_func_len = (addr_t)&__initfuncs_end - (addr_t)&__initfuncs_begin;
	struct INIT_FUNC** ifn_chain = kmalloc(init_func_len);
	memcpy(ifn_chain, (void*)&__initfuncs_begin, init_func_len);

	/* Sort the init functions chain; we use a simple bubble sort to do so */
	int num_init_funcs = init_func_len / sizeof(struct INIT_FUNC*);
	for (int i = 0; i < num_init_funcs; i++)
		for (int j = num_init_funcs - 1; j > i; j--) {
			struct INIT_FUNC* a = ifn_chain[j];
			struct INIT_FUNC* b = ifn_chain[j - 1];
			if ((a->if_subsystem > b->if_subsystem) ||
			    (a->if_subsystem >= b->if_subsystem &&
			     a->if_order >= b->if_order))
				continue;
			struct INIT_FUNC swap;
			swap = *a;
			*a = *b;
			*b = swap;
		}

#ifdef VERBOSE_INIT
	kprintf("Init tree\n");
	struct INIT_FUNC** c = (struct INIT_FUNC**)ifn_chain;
	for (size_t s = 0; s < init_func_len; s += sizeof(void*), c++) {
		kprintf("initfunc %u -> %p (subsys %x, order %x)\n", s / sizeof(void*),
		 (*c)->if_func, (*c)->if_subsystem, (*c)->if_order);
	}
#endif
	
	/* Execute all init functions in order */
	struct INIT_FUNC** ifn = (struct INIT_FUNC**)ifn_chain;
	for (size_t s = sizeof(void*); s < init_func_len; s += sizeof(void*), ifn++)
		(*ifn)->if_func();

	/* Execute the final function; we expect it will not return */
	struct INIT_FUNC* ifunc = *ifn;
	kfree(ifn_chain);
	ifunc->if_func();
	panic("init chain returned");
}

static errorcode_t
hello_world()
{
	/* Show a startup banner */
	size_t mem_avail, mem_total;
	kmem_stats(&mem_avail, &mem_total);
	kprintf("Ananas/%s - %s %s\n", ARCHITECTURE, __DATE__, __TIME__);
	kprintf("Memory: %uKB available / %uKB total\n", mem_avail / 1024, mem_total / 1024);
#if defined(__i386__) || defined(__amd64__)
	extern int md_cpu_clock_mhz;
	kprintf("CPU: %u MHz\n", md_cpu_clock_mhz);
#endif

	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(hello_world, SUBSYSTEM_CONSOLE, ORDER_LAST);

static errorcode_t
mount_filesystems()
{
	errorcode_t err;

	kprintf("- Mounting / from %s...", ROOT_DEVICE);
	err = vfs_mount(ROOT_DEVICE, "/", ROOT_FS_TYPE, NULL);
	if (err == ANANAS_ERROR_NONE) {
		kprintf(" ok\n");
	} else {
		kprintf(" failed, error %i\n", err);
	}

#ifdef DEVFS
	kprintf("- Mounting devfs on %s...", DEVFS_MOUNTPOINT);
	err = vfs_mount(NULL, DEVFS_MOUNTPOINT, "devfs", NULL);
	if (err == ANANAS_ERROR_NONE) {
		kprintf(" ok\n");
	} else {
		kprintf(" failed, error %i\n", err);
	}
#endif

#ifdef RAMDISK
	kprintf("- Mounting romdisk on /rom...");
	err = vfs_mount(RAMDISK, "/rom", "cramfs", NULL);
	if (err == ANANAS_ERROR_NONE) {
		kprintf(" ok\n");
	} else {
		kprintf(" failed, error %i\n", err);
	}
#endif
	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(mount_filesystems, SUBSYSTEM_VFS, ORDER_LAST);

#ifdef SHELL_BIN
static errorcode_t
launch_shell()
{
	thread_t* t;
	errorcode_t err = thread_alloc(NULL, &t);
	if (err != ANANAS_ERROR_NONE) {
		kprintf(" couldn't create process, %i\n", err);
		return err;
	}

	struct VFS_FILE f;
	kprintf("- Lauching %s...", SHELL_BIN);
	err = vfs_open(SHELL_BIN, NULL, &f);
	if (err == ANANAS_ERROR_NONE) {
		err = elf_load_from_file(t, f.f_inode);
		if (err == ANANAS_ERROR_NONE) {
			kprintf(" ok\n");
			thread_set_args(t, "sh\0\0", PAGE_SIZE);
			thread_set_environment(t, "OS=Ananas\0USER=root\0\0", PAGE_SIZE);
			thread_resume(t);
		} else {
			kprintf(" fail - error %i\n", err);
		}
		vfs_close(&f);
	} else {
		kprintf(" fail - error %i\n", err);
	}
	return err;
}

INIT_FUNCTION(launch_shell, SUBSYSTEM_SCHEDULER, ORDER_MIDDLE);
#endif /* SHELL_BIN  */

void
mi_startup()
{
	/* Initialize kernel symbols; these will be required very soon once we have modules */
	symbols_init();

	/*
	 * Cheat and initialize the console driver first; this ensures the user
	 * will be able to see the initialization messages ;-)
	 */
	tty_preinit();
	console_init();

	/* Initialize modules */
	module_init();

	/* Run the entire init tree - won't return */
	run_init();

	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
