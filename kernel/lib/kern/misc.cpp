#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/schedule.h"
#include "options.h"
#ifdef OPTION_SMP
#include "kernel/x86/smp.h" // XXX
#endif
#include "kernel-md/atomic.h"
#include "kernel-md/interrupts.h"

static atomic_t kdb_panicing;

void
_panic(const char* file, const char* func, int line, const char* fmt, ...)
{
	va_list ap;
	md_interrupts_disable();

#ifdef OPTION_SMP
	/*
	 * Ensure our other CPU's do not continue - we do not want to make things
	 * worse.
	 */
	smp_panic_others();
#endif

	/*
	 * Do our best to detect double panics; this will at least give more of a
	 * clue what's going on instead of making it worse.
	 */
	if (atomic_read(&kdb_panicing) == (int)(addr_t)&_panic) {
		md_interrupts_disable();
		kprintf("double panic: %s:%u (%s) - dying!\n", file, line, func);
		for(;;);
	}
	atomic_set(&kdb_panicing, (int)(addr_t)&_panic);

	/* disable the scheduler - this ensures any extra BSP's will not run threads either */
	scheduler_deactivate();

	kprintf("panic in %s:%u (%s): ", file, line, func);

	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);
	kprintf("\n");

#ifdef OPTION_GDB
	__asm("ud2");
#endif

#ifdef OPTION_KDB
	kdb_panic();
#endif
	for(;;);
}

extern "C" void __cxa_pure_virtual()
{
	panic("pure virtual call");
}

/* vim:set ts=2 sw=2: */
