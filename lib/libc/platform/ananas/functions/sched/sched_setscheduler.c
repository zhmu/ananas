#include <sched.h>
#include <errno.h>

int
sched_setscheduler(pid_t pid, int policy, const struct sched_param* param)
{
	errno = ENOSYS;
	return -1;
}
