#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/thread.h>
#include <ananas/trace.h>

TRACE_SETUP;

void
sys_exit(int exitcode)
{
	TRACE(SYSCALL, FUNC, "t=%p, exitcode=%u", PCPU_GET(curthread), exitcode);
	thread_exit(THREAD_MAKE_EXITCODE(THREAD_TERM_SYSCALL, exitcode));
}
/* vim:set ts=2 sw=2: */
