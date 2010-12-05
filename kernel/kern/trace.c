#include <ananas/types.h>
#include <ananas/lib.h>
#include <ananas/trace.h>

uint32_t trace_subsystem_mask[TRACE_SUBSYSTEM_LAST] = { 0 };

void
tracef(int fileid, const char* func, const char* fmt, ...)
{
	/* XXX This function is a SMP-disaster for now */
  va_list ap;
	kprintf("%s: ", func);
  va_start(ap, fmt);
  vaprintf(fmt, ap);
  va_end(ap);
	kprintf("\n");
}

/* vim:set ts=2 sw=2: */
