#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int link(const char* oldpath, const char* newpath)
{
    statuscode_t status = sys_link(oldpath, newpath);
    return map_statuscode(status);
}
