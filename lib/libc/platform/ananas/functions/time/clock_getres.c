#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <time.h>
#include "_map_statuscode.h"

int clock_getres(clockid_t id, struct timespec* res)
{
	statuscode_t status = sys_clock_getres(id, res);
	return map_statuscode(status);
}
