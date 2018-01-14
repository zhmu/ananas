#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/trace.h"

TRACE_SETUP;

struct Thread;

errorcode_t sys_link(Thread* t, const char* oldpath, const char* newpath)
{
	TRACE(SYSCALL, FUNC, "t=%p, oldpath='%s' newpath='%s'", t, oldpath, newpath);

	return ANANAS_ERROR(UNKNOWN);
}
