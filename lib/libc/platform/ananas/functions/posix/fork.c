#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

pid_t fork()
{
    statuscode_t status = sys_clone(0);
    return map_statuscode(status);
}
