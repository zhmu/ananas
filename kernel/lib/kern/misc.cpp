#include <ananas/util/atomic.h>
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/schedule.h"
#include "options.h"
#ifdef OPTION_SMP
#include "kernel/x86/smp.h" // XXX
#endif
#include "kernel-md/interrupts.h"

namespace {

constexpr addr_t kdb_no_panic = 0;
util::atomic<addr_t> kdb_panicing{kdb_no_panic};

}

void
_panic(const char* file, const char* func, int line, const char* fmt, ...)
{
	va_list ap;
	md::interrupts::Disable();

#ifdef OPTION_SMP
	/*
	 * Ensure our other CPU's do not continue - we do not want to make things
	 * worse.
	 */
	smp_panic_others();
#endif

	/*
	 * Do our best to detect double panics; this will at least give more of a
	 * clue what's going on instead of making it worse. Note that we use the
	 * strong compare/exchange here as we do not want to deal with retries.
	 */
	addr_t expected = kdb_no_panic;
	if (!kdb_panicing.compare_exchange_strong(expected, reinterpret_cast<addr_t>(&_panic))) {
		kprintf("double panic: %s:%u (%s) - dying!\n", file, line, func);
		for(;;)
			/* nothing */;
	}

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
