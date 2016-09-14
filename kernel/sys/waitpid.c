#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/process.h>
#include <ananas/thread.h>
#include <ananas/trace.h>

#include <ananas/lib.h>

TRACE_SETUP;

errorcode_t
sys_waitpid(thread_t* t, pid_t* pid, int* stat_loc, int options)
{
	TRACE(SYSCALL, FUNC, "t=%p, pid=%d stat_loc=%p options=%d", t, pid, stat_loc, options);

	process_t* p;
	errorcode_t err = process_wait_and_lock(t->t_process, options, &p);
	ANANAS_ERROR_RETURN(err);

	*pid = p->p_pid;
	process_unlock(p);

	/* Give up our refence to the zombie child; this should destroy it */
	process_deref(p);

	return ANANAS_ERROR_NONE;
}

/* vim:set ts=2 sw=2: */
