#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include "kernel/trace.h"

TRACE_SETUP;

errorcode_t
sys_utime(thread_t* t, const char* path, const struct utimbuf* times)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s' times=%p", t, path, times);

	return ANANAS_ERROR(UNKNOWN);
}
