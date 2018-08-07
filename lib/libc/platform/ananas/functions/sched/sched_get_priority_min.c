#include <sched.h>
#include <errno.h>

int
sched_get_priority_min(int policy)
{
	errno = ENOSYS;
	return -1;
}
