#include <ananas/lib.h>
#include <ananas/trace.h>
#include <stdio.h>
#include <stdlib.h>

void* __traceid_begin;
void* __traceid_end;
uint32_t trace_subsystem_mask[TRACE_SUBSYSTEM_LAST] = { 0 };

void
kprintf(const char* fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
}

void
tracef(int fileid, const char* func, const char* fmt, ...)
{
  va_list ap;
	kprintf("%s: ", func);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
	kprintf("\n");
}

/* There is no need to lock anything, so just stub the locking stuff out */
struct SPINLOCK;
void md_spinlock_init(struct SPINLOCK* s) {}
void md_spinlock_lock(struct SPINLOCK* s) {}
void md_spinlock_unlock(struct SPINLOCK* s) {}

void
md_reschedule()
{
	panic("md_reschedule");
}

void
_panic(const char* file, const char* func, int line, const char* fmt, ...)
{
	va_list ap;

	printf("panic in %s:%u (%s): ", file, line, func);

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
	abort();
}

void
vaprintf(const char* fmt, va_list va)
{
	vprintf(fmt, va);
}

void
vm_map(void* ptr, size_t len)
{
}

/* vim:set ts=2 sw=2: */
