#include <ananas/syscalls.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

Result
sys_waitpid(Thread* t, pid_t* pid, int* stat_loc, int options)
{
	TRACE(SYSCALL, FUNC, "t=%p, pid=%d stat_loc=%p options=%d", t, pid, stat_loc, options);

	Process* p;
	RESULT_PROPAGATE_FAILURE(
		t->t_process->WaitAndLock(options, p)
	);

	*pid = p->p_pid;
	if (stat_loc != nullptr)
		*stat_loc = p->p_exit_status;
	p->Unlock();

	/* Give up our refence to the zombie child; this should destroy it */
	p->Deref();

	return Result::Success();
}

/* vim:set ts=2 sw=2: */
