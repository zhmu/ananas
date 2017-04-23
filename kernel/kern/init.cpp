#include <ananas/device.h>
#include <ananas/console.h>
#include <ananas/cmdline.h>
#include <ananas/error.h>
#include <ananas/exec.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <ananas/init.h>
#include <ananas/thread.h>
#include <ananas/process.h>
#include <ananas/pcpu.h>
#include <ananas/tty.h>
#include <ananas/vfs.h>
#include <machine/vm.h>
#include <machine/param.h> /* for PAGE_SIZE */

#include "options.h"

/* If set, display the entire init tree before launching it */
#undef VERBOSE_INIT

extern void* __initfuncs_begin;
extern void* __initfuncs_end;

static struct INIT_FUNC_DYNAMICS initfunc_dynamics;
static int initfunc_dynamics_amount = 0;

void
init_register_func(struct KERNEL_MODULE* kmod, struct INIT_FUNC* ifunc)
{
	auto idfunc = new INIT_DYNAMIC_FUNC;
	idfunc->idf_kmod = kmod;
	idfunc->idf_ifunc = ifunc;
	/* No lock necessary as we're behind the module lock either way */
	LIST_APPEND(&initfunc_dynamics, idfunc);
	initfunc_dynamics_amount++;
#ifdef VERBOSE_INIT	
	kprintf("init_register_func(): kmod=%p ifunc=%p\n", kmod, ifunc->if_func);
#endif
}

void
init_unregister_module(struct KERNEL_MODULE* kmod)
{
	/* No lock necessary as we're behind the module lock either way */
	LIST_FOREACH_SAFE(&initfunc_dynamics, idf, struct INIT_DYNAMIC_FUNC) {
		if (idf->idf_kmod != kmod)
			continue;
		LIST_REMOVE(&initfunc_dynamics, idf);
		kfree(idf);
	}
}

static void
run_init(enum INIT_SUBSYSTEM first_subsystem, enum INIT_SUBSYSTEM last_subsystem)
{
	/*
	 * Create a shadow copy of the init function chain; this is done so that we
	 * can sort it.
	 */
	size_t init_static_func_len  = (addr_t)&__initfuncs_end - (addr_t)&__initfuncs_begin;
	kprintf("init_static_func_len = %d\n", init_static_func_len);
	struct INIT_FUNC** ifn_chain = static_cast<struct INIT_FUNC**>(kmalloc(init_static_func_len + initfunc_dynamics_amount * sizeof(struct INIT_FUNC*)));
	memcpy(ifn_chain, (void*)&__initfuncs_begin, init_static_func_len);
	int n = init_static_func_len / sizeof(struct INIT_FUNC*);
	if (n > 0)
		LIST_FOREACH(&initfunc_dynamics, idf, struct INIT_DYNAMIC_FUNC) {
			ifn_chain[n++] = idf->idf_ifunc;
		}
	
	/* Sort the init functions chain; we use a simple bubble sort to do so */
	int num_init_funcs = init_static_func_len / sizeof(struct INIT_FUNC*) + initfunc_dynamics_amount;
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
	for (int n = 0; n < num_init_funcs; n++, c++) {
		kprintf("initfunc %u -> %p (subsys %x, order %x)", n,
		 (*c)->if_func, (*c)->if_subsystem, (*c)->if_order);
		if ((*c)->if_subsystem < first_subsystem)
			kprintf(" [skip]");
		else if ((*c)->if_subsystem > last_subsystem)
			kprintf(" [ignore]");
		kprintf("\n");
	}
#endif
	
	/* Execute all init functions in order except the final one */
	struct INIT_FUNC** ifn = (struct INIT_FUNC**)ifn_chain;
	for (int i = 0; i < num_init_funcs - 1; i++, ifn++) {
		if ((*ifn)->if_subsystem < first_subsystem)
			continue;
		if ((*ifn)->if_subsystem > last_subsystem)
			break;
		(*ifn)->if_func();
	}

	/* Throw away the init function chain; it served its purpose */
	struct INIT_FUNC* ifunc = *ifn;
	kfree(ifn_chain);

	/* Only throw away the dynamic chain if this is our final init run */
	if (last_subsystem == SUBSYSTEM_LAST)
		LIST_FOREACH_SAFE(&initfunc_dynamics, idf, struct INIT_DYNAMIC_FUNC) {
			kfree(idf);
		}

	/* Call the final init function; it shouldn't return */
	ifunc->if_func();
}

static errorcode_t
hello_world()
{
	/* Show a startup banner */
	kprintf("Ananas/%s - %s %s\n", ARCHITECTURE, __DATE__, __TIME__);
	unsigned int total_pages, avail_pages;
	page_get_stats(&total_pages, &avail_pages);
	kprintf("Memory: %uKB available / %uKB total\n", avail_pages * (PAGE_SIZE / 1024), total_pages * (PAGE_SIZE / 1024));
#if defined(__i386__) || defined(__amd64__)
	extern int md_cpu_clock_mhz;
	kprintf("CPU: %u MHz\n", md_cpu_clock_mhz);
#endif

	return ananas_success();
}

INIT_FUNCTION(hello_world, SUBSYSTEM_CONSOLE, ORDER_LAST);

static errorcode_t
mount_filesystems()
{
	errorcode_t err;

	/* We expect root=type:device here */
	const char* rootfs_arg = cmdline_get_string("root");
	if (rootfs_arg == NULL)
		return ananas_success();

	char rootfs_type[64];
	const char* p = strchr(rootfs_arg, ':');
	if (p == NULL)
		return ananas_success();

	memcpy(rootfs_type, rootfs_arg, p - rootfs_arg);
	rootfs_type[p - rootfs_arg] = '\0';
	const char* rootfs = p + 1;

	kprintf("- Mounting / (type %s) from %s...", rootfs_type, rootfs);
	err = vfs_mount(rootfs, "/", rootfs_type, NULL);
	if (ananas_is_success(err)) {
		kprintf(" ok\n");
	} else {
		kprintf(" failed, error %i\n", err);
	}

	return ananas_success();
}

INIT_FUNCTION(mount_filesystems, SUBSYSTEM_VFS, ORDER_LAST);

static errorcode_t
launch_init()
{
	errorcode_t err = ananas_success();
	process_t* proc;
	thread_t* t;

	const char* init_path = cmdline_get_string("init");
	if (init_path == NULL || init_path[0] == '\0')
		return err;

	err = process_alloc(NULL, &proc);
	if (ananas_is_failure(err)) {
		kprintf(" couldn't create process, %i\n", err);
		return err;
	}
	err = thread_alloc(proc, &t, "init", THREAD_ALLOC_DEFAULT);
	process_deref(proc); /* 't' should have a ref, we don't need it anymore */
	if (ananas_is_failure(err)) {
		kprintf(" couldn't create thread, %i\n", err);
		return err;
	}

	kprintf("- Lauching init from %s...", init_path);
	struct VFS_FILE file;
	err = vfs_open(init_path, proc->p_cwd, &file);
	if (ananas_is_failure(err)) {
		kprintf(" couldn't open init executable, %i\n", err);
		return err;
	}

	const char args[] = "init\0\0";
	const char env[] = "OS=Ananas\0USER=root\0\0";
	process_set_args(proc, args, sizeof(args));
	process_set_environment(proc, env, sizeof(env));

	addr_t exec_addr;
	err = exec_load(proc->p_vmspace, file.f_dentry, &exec_addr);
	if (ananas_is_success(err)) {
		kprintf(" ok\n");
		md_setup_post_exec(t, exec_addr);
		thread_resume(t);
	} else {
		kprintf(" fail - error %i\n", err);
	}
	vfs_close(&file);
	return err;
}

INIT_FUNCTION(launch_init, SUBSYSTEM_SCHEDULER, ORDER_MIDDLE);

static void
init_thread_func(void* done)
{
	/*
	 * We need to run the init tree in two steps: up to the module part, and everything
	 * after it; this is done so that modules can dynamically register functions as
	 * well.
	 */
	run_init(SUBSYSTEM_MODULE, static_cast<INIT_SUBSYSTEM>(SUBSYSTEM_MODULE + 1));
	run_init(static_cast<INIT_SUBSYSTEM>(SUBSYSTEM_MODULE + 1), SUBSYSTEM_LAST);

	/* All done - signal and exit - the reaper will clean up this thread */
	*(volatile int*)done = 1;
	thread_exit(0);
	/* NOTREACHED */
}

void
mi_startup()
{
	LIST_INIT(&initfunc_dynamics);

	/*
	 * Create a thread to handle the init functions; this will allow us to
	 * sleep whenever we want, as there is always the idle thread to pick up the
	 * cycles for us (note that this code is run as the init thread and such it
	 * must never sleep)
	 */
	volatile int done = 0;
	thread_t init_thread;
	kthread_init(&init_thread, "init", init_thread_func, (void*)&done);
	thread_resume(&init_thread);

	/* Activate the scheduler - it is time */
	scheduler_launch();

	/*
	 *  Okay, for time being this will be the idle thread - we must not sleep, as
	 * we are the idle thread
	 */
	while(!done) {
		md_cpu_relax();
	}

	/* And now, we become the idle thread */
	idle_thread(nullptr);

	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
