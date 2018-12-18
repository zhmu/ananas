#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <sys/stat.h>
#include <unistd.h>
#include "_map_statuscode.h"

int fstat(int fd, struct stat* buf)
{
    statuscode_t status = sys_fstat(fd, buf);
    return map_statuscode(status);
}
