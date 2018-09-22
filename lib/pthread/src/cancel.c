#include <pthread.h>
#include <errno.h>

int pthread_cancel(pthread_t thread)
{
	errno = ENOSYS;
	return -1;
}

int pthread_setcancelstate(int state, int* oldstate)
{
	errno = ENOSYS;
	return -1;
}

int pthread_setcanceltype(int type, int* oldtype)
{
	errno = ENOSYS;
	return -1;
}

void pthread_testcancel(void)
{
}
