#include <sys/mount.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int fstatfs(int fildes, struct statfs* buf)
{
    statuscode_t status = sys_fstatfs(fildes, buf);
    return map_statuscode(status);
}
