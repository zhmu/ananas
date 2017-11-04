#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include "kernel/time.h"
#include "kernel/trace.h"

#include "kernel/lib.h"

TRACE_SETUP;

errorcode_t
sys_clock_settime(thread_t* t, int id, const struct timespec* tp)
{
	TRACE(SYSCALL, FUNC, "t=%p, id=%d", t, id);
	// XXX We don't support setting the time just yet
	return ANANAS_ERROR(UNKNOWN);
}

errorcode_t
sys_clock_gettime(thread_t* t, int id, struct timespec* tp)
{
	TRACE(SYSCALL, FUNC, "t=%p, id=%d", t, id);
	switch(id) {
		case CLOCK_MONOTONIC:
			break;
		case CLOCK_REALTIME: {
			*tp = Ananas::Time::GetTime();
			return ananas_success();
		}
		case CLOCK_SECONDS:
			break;
	}
	return ANANAS_ERROR(UNKNOWN);
}

errorcode_t
sys_clock_getres(thread_t* t, int id, struct timespec* res)
{
	TRACE(SYSCALL, FUNC, "t=%p, id=%d", t, id);
	return ANANAS_ERROR(UNKNOWN);
}

/* vim:set ts=2 sw=2: */
