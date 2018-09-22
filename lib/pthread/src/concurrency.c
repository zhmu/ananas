#include <pthread.h>
#include <errno.h>

int pthread_setconcurrency(int new_level)
{
	errno = ENOSYS;
	return -1;
}

int pthread_getconcurrency(void)
{
	errno = ENOSYS;
	return -1;
}

