#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <signal.h>
#include "_map_statuscode.h"

int kill(pid_t pid, int sig)
{
    statuscode_t status = sys_kill(pid, sig);
    return map_statuscode(status);
}
