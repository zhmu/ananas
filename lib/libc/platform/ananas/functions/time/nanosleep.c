#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <time.h>
#include "_map_statuscode.h"

int nanosleep(const struct timespec* rqtp, struct timespec* rmtp)
{
    statuscode_t status = sys_nanosleep(rqtp, rmtp);
    return map_statuscode(status);
}
