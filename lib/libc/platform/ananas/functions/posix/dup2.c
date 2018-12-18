#include <ananas/types.h>
#include <ananas/handle-options.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int dup2(int fildes, int fildes2)
{
    statuscode_t status = sys_dup2(fildes, fildes2);
    return map_statuscode(status);
}

/* vim:set ts=2 sw=2: */
