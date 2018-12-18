#include <ananas/types.h>
#include <ananas/statuscode.h>
#include <ananas/syscalls.h>
#include <errno.h>
#include <stdarg.h>
#include "_map_statuscode.h"

int ioctl(int fd, unsigned long request, ...)
{
    va_list va;
    va_start(va, request);
    void* arg1 = va_arg(va, void*);
    void* arg2 = va_arg(va, void*);
    void* arg3 = va_arg(va, void*);
    va_end(va);

    statuscode_t status = sys_ioctl(fd, request, arg1, arg2, arg3);
    return map_statuscode(status);
}
