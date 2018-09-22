#include <pthread.h>
#include <errno.h>

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void))
{
	errno = ENOSYS;
	return -1;
}
