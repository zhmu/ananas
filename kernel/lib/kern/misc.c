#include <ananas/lib.h>
#include <ananas/schedule.h>
#include <ananas/kdb.h>
#include "options.h"

void
_panic(const char* file, const char* func, int line, const char* fmt, ...)
{
	va_list ap;

	/* disable the scheduler - this ensures any extra BSP's will not run threads either */
	scheduler_deactivate();

	kprintf("panic in %s:%u (%s): ", file, line, func);

	va_start(ap, fmt);
	vaprintf(fmt, ap);
	va_end(ap);
	kprintf("\n");

#ifdef KDB
	kdb_panic();
#else
	while(1);
#endif
}

/* vim:set ts=2 sw=2: */
