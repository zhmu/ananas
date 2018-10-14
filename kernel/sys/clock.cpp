#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/syscalls.h>
#include "kernel/result.h"
#include "kernel/time.h"
#include "kernel/trace.h"

#include "kernel/lib.h"

TRACE_SETUP;

Result
sys_clock_settime(Thread* t, int id, const struct timespec* tp)
{
	TRACE(SYSCALL, FUNC, "t=%p, id=%d", t, id);
	// XXX We don't support setting the time just yet
	return RESULT_MAKE_FAILURE(EINVAL);
}

Result
sys_clock_gettime(Thread* t, int id, struct timespec* tp)
{
	TRACE(SYSCALL, FUNC, "t=%p, id=%d", t, id);
	switch(id) {
		case CLOCK_MONOTONIC:
			break;
		case CLOCK_REALTIME: {
			*tp = time::GetTime();
			return Result::Success();
		}
		case CLOCK_SECONDS:
			break;
	}
	return RESULT_MAKE_FAILURE(EINVAL);
}

Result
sys_clock_getres(Thread* t, int id, struct timespec* res)
{
	TRACE(SYSCALL, FUNC, "t=%p, id=%d", t, id);
	return RESULT_MAKE_FAILURE(EINVAL);
}

/* vim:set ts=2 sw=2: */
