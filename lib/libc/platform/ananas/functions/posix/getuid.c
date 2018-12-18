#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

uid_t getuid()
{
    statuscode_t status = sys_getuid();
    return map_statuscode(status);
}
