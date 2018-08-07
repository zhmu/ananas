#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/trace.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_nanosleep(Thread* t, const struct timespec* rqtp, struct timespec* rmtp)
{
	TRACE(SYSCALL, FUNC, "t=%p, rqtp=%p, rmtp%p", t, rqtp, rmtp);

	return RESULT_MAKE_FAILURE(EINVAL); // TODO
}
