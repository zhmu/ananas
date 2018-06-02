#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/process.h"
#include "kernel/processgroup.h"
#include "kernel/trace.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_getpgrp(Thread* t)
{
	TRACE(SYSCALL, FUNC, "t=%p", t);
	Process& process = *t->t_process;

	auto pgid = process.p_group->pg_id;
	return Result::Success(pgid);
}

Result
sys_setpgid(Thread* t, pid_t pid, pid_t pgid)
{
	TRACE(SYSCALL, FUNC, "t=%p, pid=%d pgid=%d", t, pid, pgid);
	// TODO implement this
	return RESULT_MAKE_FAILURE(EPERM);
}

Result
sys_setsid(Thread* t)
{
	TRACE(SYSCALL, FUNC, "t=%p", t);
	Process& process = *t->t_process;

	// Reject if we already are a group leader
	if (process.p_group->pg_session.s_leader == &process)
		return RESULT_MAKE_FAILURE(EPERM);

	// Exit out current process group and start a new one; this creates a new session.
	// which is what we want
	process::AbandonProcessGroup(process);
	process::InitializeProcessGroup(process, nullptr);
	return Result::Success();
}
