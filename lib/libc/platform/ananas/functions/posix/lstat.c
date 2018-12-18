#include <ananas/types.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int lstat(const char* path, struct stat* buf)
{
    statuscode_t status = sys_lstat(path, buf);
    return map_statuscode(status);
}
