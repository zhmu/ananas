#include <pthread.h>
#include <errno.h>

int pthread_getschedparam(pthread_t thread, int* policy, struct sched_param* param)
{
	errno = ENOSYS;
	return -1;
}

int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param* param)
{
	errno = ENOSYS;
	return -1;
}

