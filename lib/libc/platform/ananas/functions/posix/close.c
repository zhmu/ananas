#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int close(int fd)
{
    statuscode_t status = sys_close(fd);
    return map_statuscode(status);
}
