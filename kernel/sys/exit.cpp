#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

void
sys_exit(Thread* t, int exitcode)
{
	TRACE(SYSCALL, FUNC, "t=%p, exitcode=%u", t, exitcode);

	thread_exit(THREAD_MAKE_EXITCODE(THREAD_TERM_SYSCALL, exitcode));
}
