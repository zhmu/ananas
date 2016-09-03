#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/process.h>
#include <ananas/thread.h>
#include <ananas/trace.h>

TRACE_SETUP;

errorcode_t
sys_clone(thread_t* t, int flags, pid_t* out_pid)
{
	TRACE(SYSCALL, FUNC, "t=%p, flags=0x%x, out_pid=%p", t, flags, out_pid);
	errorcode_t err;
	process_t* proc = t->t_process;

	/* XXX Future improvement so we can do vfork() and such */
	if (flags != 0)
		return ANANAS_ERROR(BAD_FLAG);

	/* First, make a copy of the process; this inherits all files and such */
	process_t* new_proc;
	err = process_clone(proc, 0, &new_proc);
	ANANAS_ERROR_RETURN(err);

	/* Now clone the handle to the new process */
	thread_t* new_thread;
	err = thread_clone(new_proc, &new_thread);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	*out_pid = new_proc->p_pid;

	/* Resume the cloned thread - it'll have a different return value from ours */
	thread_resume(new_thread);

	TRACE(SYSCALL, FUNC, "t=%p, success, new pid=%u", *out_pid);
	return err;

fail:
	process_deref(new_proc);
	return err;
}

/* vim:set ts=2 sw=2: */
