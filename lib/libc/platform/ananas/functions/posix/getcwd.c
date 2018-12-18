#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

char* getcwd(char* buf, size_t size)
{
    statuscode_t status = sys_getcwd(buf, size);
    if (ananas_statuscode_is_success(status))
        return buf;

    map_statuscode(status);
    return NULL;
}
