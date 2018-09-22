#include <pthread.h>
#include <errno.h>

int pthread_mutexattr_init(pthread_mutexattr_t* attr)
{
	errno = ENOSYS;
	return -1;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr)
{
	errno = ENOSYS;
	return -1;
}

int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t* attr, int* prioceiling)
{
	errno = ENOSYS;
	return -1;
}

int pthread_mutexattr_setprioceiling(pthread_mutexattr_t* attr, int prioceiling)
{
	errno = ENOSYS;
	return -1;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t* attr, int* protocol)
{
	errno = ENOSYS;
	return -1;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t* attr, int protocol)
{
	errno = ENOSYS;
	return -1;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t* attr, int* pshared)
{
	errno = ENOSYS;
	return -1;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int psahred)
{
	errno = ENOSYS;
	return -1;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type)
{
	errno = ENOSYS;
	return -1;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type)
{
	errno = ENOSYS;
	return -1;
}
