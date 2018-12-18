#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int setsid()
{
    statuscode_t status = sys_setsid(0);
    return map_statuscode(status);
}
