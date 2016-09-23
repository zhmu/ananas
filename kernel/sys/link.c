#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscall.h>
#include <ananas/trace.h>

TRACE_SETUP;

errorcode_t sys_link(thread_t* t, const char* oldpath, const char* newpath)
{
	TRACE(SYSCALL, FUNC, "t=%p, oldpath='%s' newpath='%s'", t, oldpath, newpath);

	return ANANAS_ERROR(UNKNOWN);
}
