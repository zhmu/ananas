#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/thread.h>

void
sys_exit(int exitcode)
{
	thread_exit(THREAD_MAKE_EXITCODE(THREAD_TERM_SYSCALL, exitcode));
}
/* vim:set ts=2 sw=2: */
