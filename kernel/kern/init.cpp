#include <ananas/device.h>
#include <ananas/console.h>
#include <ananas/error.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <ananas/init.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/tty.h>
#include <machine/vm.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include "options.h" // for ARCHITECTURE

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
	size_t init_static_func_len  = (addr_t)&__initfuncs_end - (addr_t)&__initfuncs_begin;
	struct INIT_FUNC** ifn_chain = static_cast<struct INIT_FUNC**>(kmalloc(init_static_func_len));
	memcpy(ifn_chain, (void*)&__initfuncs_begin, init_static_func_len);
	
	/* Sort the init functions chain; we use a simple bubble sort to do so */
	int num_init_funcs = init_static_func_len / sizeof(struct INIT_FUNC*);
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
		kprintf("initfunc %u -> %p (subsys %x, order %x)\n", n,
		 (*c)->if_func, (*c)->if_subsystem, (*c)->if_order);
	}
#endif
	
	/* Execute all init functions in order except the final one */
	struct INIT_FUNC** ifn = (struct INIT_FUNC**)ifn_chain;
	for (int i = 0; i < num_init_funcs - 1; i++, ifn++)
		(*ifn)->if_func();

	/* Throw away the init function chain; it served its purpose */
	struct INIT_FUNC* ifunc = *ifn;
	kfree(ifn_chain);

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

static void
init_thread_func(void* done)
{
	run_init();

	/* All done - signal and exit - the reaper will clean up this thread */
	*(volatile int*)done = 1;
	thread_exit(0);
	/* NOTREACHED */
}

void
mi_startup()
{
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
