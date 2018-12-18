#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int getsid(pid_t pid)
{
    statuscode_t status = sys_getsid(pid);
    return map_statuscode(status);
}
