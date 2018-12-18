#include <ananas/types.h>
#include <ananas/syscalls.h>

void _exit(int status)
{
    sys_exit(status);
    /* In case we live through this, force an endless loop to avoid 'function may return' error */
    for (;;)
        ;
}
