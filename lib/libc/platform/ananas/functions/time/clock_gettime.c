#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <time.h>
#include "_map_statuscode.h"

int clock_gettime(clockid_t id, struct timespec* ts)
{
    statuscode_t status = sys_clock_gettime(id, ts);
    return map_statuscode(status);
}
