#include <pthread.h>
#include <errno.h>

int pthread_rwlockattr_init(pthread_rwlockattr_t* attr)
{
    errno = ENOSYS;
    return -1;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t* attr)
{
    errno = ENOSYS;
    return -1;
}

int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t* attr, int* pshared)
{
    errno = ENOSYS;
    return -1;
}

int pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr, int pshared)
{
    errno = ENOSYS;
    return -1;
}
