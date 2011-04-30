#include <ananas/lib.h>
#include <ananas/trace.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void* __traceid_begin;
void* __traceid_end;
uint32_t trace_subsystem_mask[TRACE_SUBSYSTEM_LAST] = { 0 };

#define CONSOLE_LEN 128

static int  console_pos = 0;
static char console_buf[CONSOLE_LEN];

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
void md_spinlock_init(struct SPINLOCK* s) {}
void md_spinlock_lock(struct SPINLOCK* s) {}
void md_spinlock_unlock(struct SPINLOCK* s) {}

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

void
console_putchar(int c)
{
	assert(console_pos < CONSOLE_LEN);
	console_buf[console_pos++] = c;
}

const char*
console_getbuf()
{
	console_buf[console_pos] = '\0';
	return console_buf;
}

void console_reset()
{
	console_pos = 0;
}

/* vim:set ts=2 sw=2: */
