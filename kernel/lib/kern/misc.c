#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/kdb.h>
#include "options.h"

void
_panic(const char* file, const char* func, int line, const char* fmt, ...)
{
	va_list ap;
#ifndef KDB
	/* disable the scheduler - this ensures any extra BSP's will not run threads either */
	scheduler_deactivate();
#endif

	kprintf("panic in %s:%u (%s): ", file, line, func);

	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);
	kprintf("\n");

#ifdef KDB
	/*
	 * For the KDB case, disable the current thread and enter the debugger; note
	 * that this cannot be done if there is no current thread (i.e. in early
	 * bootup) so we'll just halt there for the time being XXX
	 */
	struct THREAD* curthread = PCPU_GET(curthread);
	if (curthread != NULL) {
		thread_suspend(curthread);
		kdb_enter("panic");
		reschedule();
	} else {
		scheduler_deactivate();
		while(1);
	}
#else
	while(1);
#endif
}

/* vim:set ts=2 sw=2: */
