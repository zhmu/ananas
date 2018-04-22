#include <ananas/syscalls.h>
#include <ananas/errno.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

Result
sys_clone(Thread* t, int flags, pid_t* out_pid)
{
	TRACE(SYSCALL, FUNC, "t=%p, flags=0x%x, out_pid=%p", t, flags, out_pid);
	Process& proc = *t->t_process;

	/* XXX Future improvement so we can do vfork() and such */
	if (flags != 0)
		return RESULT_MAKE_FAILURE(EINVAL);

	/* First, make a copy of the process; this inherits all files and such */
	Process* new_proc;
	RESULT_PROPAGATE_FAILURE(
		process_clone(proc, 0, new_proc)
	);

	/* Now clone the handle to the new process */
	Thread* new_thread;
	if (auto result = thread_clone(*new_proc, new_thread); result.IsFailure()) {
		process_deref(*new_proc);
		return result;
	}
	*out_pid = new_proc->p_pid;

	/* Resume the cloned thread - it'll have a different return value from ours */
	new_thread->Resume();

	TRACE(SYSCALL, FUNC, "t=%p, success, new pid=%u", t, *out_pid);
	return Result::Success();
}

/* vim:set ts=2 sw=2: */
