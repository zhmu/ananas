#include <pthread.h>
#include <errno.h>

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr)
{
	errno = ENOSYS;
	return -1;
}

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock)
{
	errno = ENOSYS;
	return -1;
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock)
{
	errno = ENOSYS;
	return -1;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock)
{
	errno = ENOSYS;
	return -1;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock)
{
	errno = ENOSYS;
	return -1;
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock)
{
	errno = ENOSYS;
	return -1;
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock)
{
	errno = ENOSYS;
	return -1;
}
