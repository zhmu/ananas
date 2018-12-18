#include <sched.h>
#include <errno.h>

int sched_rr_get_interval(pid_t pid, struct timespec* interval)
{
    errno = ENOSYS;
    return -1;
}
