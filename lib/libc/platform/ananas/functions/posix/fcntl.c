#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include "_map_statuscode.h"

int fcntl(int fildes, int cmd, ...)
{
    va_list va;
    va_start(va, cmd);

    switch (cmd) {
        case F_DUPFD:
        case F_SETFD:
        case F_SETFL:
        case F_GETFD:
        case F_GETFL: {
            statuscode_t status = sys_fcntl(fildes, cmd, (void*)(uintptr_t)va_arg(va, int), NULL);
            return map_statuscode(status);
        }
        default:
            errno = EINVAL;
            return -1;
    }
    /* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
