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

void*
kmalloc(size_t len)
{
	return malloc(len);
}

void
kfree(void* ptr)
{
	free(ptr);
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
struct MUTEX;
void spinlock_init(struct SPINLOCK* s) {}
void spinlock_lock(struct SPINLOCK* s) {}
void spinlock_unlock(struct SPINLOCK* s) {}
void mutex_init(struct MUTEX* mtx) {}
void mutex_lock(struct MUTEX* mtx) {}
void mutex_unlock(struct MUTEX* mtx) {}

/* Waitqueues aren't necessary */
struct WAITQUEUE;
void waitqueue_init(struct WAITQUEUE* wq) { }
struct WAITER* waitqueue_add(struct WAITQUEUE* wq) { return NULL; }
void waitqueue_wait(struct WAITER* w) { }
void waitqueue_remove(struct WAITER* w) { }
void waitqueue_signal(struct WAITER* w) { }

void
md_reschedule()
{
	panic("md_reschedule() must not be called");
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

/* vim:set ts=2 sw=2: */
