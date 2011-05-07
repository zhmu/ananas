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
struct SPINLOCK { int var; };
void spinlock_init(struct SPINLOCK* s) { s->var = 0;}
void spinlock_lock(struct SPINLOCK* s) { KASSERT(s->var == 0, "deadlock"); s->var = 1; }
void spinlock_unlock(struct SPINLOCK* s) { KASSERT(s->var != 0, "missing lock"); s->var = 0; }

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

void*
vm_map_kernel(void* ptr, size_t len, int flags)
{
	return ptr;
}

/* vim:set ts=2 sw=2: */
