#include <sched.h>
#include <errno.h>

int sched_getscheduler(pid_t pid)
{
    errno = ENOSYS;
    return -1;
}
