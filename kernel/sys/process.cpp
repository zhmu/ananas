#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/process.h"
#include "kernel/trace.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_getpid(Thread* t)
{
	TRACE(SYSCALL, FUNC, "t=%p", t);
	Process& process = *t->t_process;

	auto pid = process.p_pid;
	return Result::Success(pid);
}

Result
sys_getppid(Thread* t)
{
	TRACE(SYSCALL, FUNC, "t=%p", t);
	Process& process = *t->t_process->p_parent;

	auto ppid = process.p_pid;
	return Result::Success(ppid);
}

Result
sys_getuid(Thread* t)
{
	TRACE(SYSCALL, FUNC, "t=%p", t);

	auto uid = 0; // TODO
	return Result::Success(uid);
}

Result
sys_geteuid(Thread* t)
{
	TRACE(SYSCALL, FUNC, "t=%p", t);

	auto euid = 0; // TODO
	return Result::Success(euid);
}

Result
sys_getgid(Thread* t)
{
	TRACE(SYSCALL, FUNC, "t=%p", t);

	auto gid = 0; // TODO
	return Result::Success(gid);
}

Result
sys_getegid(Thread* t)
{
	TRACE(SYSCALL, FUNC, "t=%p", t);

	auto egid = 0; // TODO
	return Result::Success(egid);
}
