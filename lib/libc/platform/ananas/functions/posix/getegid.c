#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

gid_t getegid()
{
    statuscode_t status = sys_getegid();
    return map_statuscode(status);
}
