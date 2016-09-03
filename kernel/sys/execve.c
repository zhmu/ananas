#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/process.h>
#include <ananas/thread.h>
#include <ananas/trace.h>

TRACE_SETUP;

errorcode_t
sys_execve(thread_t* t, const char* path, const char** argv, const char** envp)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s', argv='%s', envp='%s'", path, argv, envp);

	return ANANAS_ERROR(BAD_HANDLE);
}

/* vim:set ts=2 sw=2: */
