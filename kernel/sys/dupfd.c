#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/process.h>
#include <ananas/thread.h>
#include <ananas/trace.h>

TRACE_SETUP;

errorcode_t
sys_dupfd(thread_t* t, handleindex_t index, int flags, handleindex_t* out)
{
	TRACE(SYSCALL, FUNC, "t=%p, index=%d, flags=%d", t, index, flags);

	return ANANAS_ERROR(BAD_HANDLE);
}

/* vim:set ts=2 sw=2: */
