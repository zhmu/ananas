#include <ananas/types.h>
#include <ananas/console.h>
#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/trace.h>

#define TRACE_PRINTF_BUFSIZE 256

uint32_t trace_subsystem_mask[TRACE_SUBSYSTEM_LAST];

void
tracef(int fileid, const char* func, const char* fmt, ...)
{
	uint32_t timestamp = 0;
#if defined(__i386__) || defined(__amd64__)
	/* XXX This should be generic somehow */
	uint32_t x86_get_ms_since_boot();
	timestamp = x86_get_ms_since_boot();
#endif

	char buf[TRACE_PRINTF_BUFSIZE];

	thread_t* curthread = PCPU_GET(curthread);
	const char* tname = curthread->t_name;

	snprintf(buf, sizeof(buf), "[%4u.%03u] (%s) %s: ", timestamp / 1000, timestamp % 1000, tname, func);

	va_list va;
	va_start(va, fmt);
	vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 2, fmt, va);
	buf[sizeof(buf) - 2] = '\0';
	strcat(buf, "\n");
	va_end(va);

	console_putstring(buf);
}

/* vim:set ts=2 sw=2: */
