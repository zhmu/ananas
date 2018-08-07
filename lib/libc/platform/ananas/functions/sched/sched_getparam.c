#include <sched.h>
#include <errno.h>

int
sched_getparam(pid_t pid, struct sched_param* param)
{
	errno = ENOSYS;
	return -1;
}
