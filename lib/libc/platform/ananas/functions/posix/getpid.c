#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

pid_t getpid()
{
    statuscode_t status = sys_getpid();
    return map_statuscode(status);
}
