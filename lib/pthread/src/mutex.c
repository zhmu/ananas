#include <pthread.h>
#include <errno.h>

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) { return 0; }

int pthread_mutex_destroy(pthread_mutex_t* mutex) { return 0; }

int pthread_mutex_lock(pthread_mutex_t* mutex) { return 0; }

int pthread_mutex_trylock(pthread_mutex_t* mutex) { return 0; }

int pthread_mutex_unlock(pthread_mutex_t* mutex) { return 0; }

int pthread_mutex_getprioceiling(const pthread_mutex_t* mutex, int* prioceiling)
{
    errno = ENOSYS;
    return -1;
}

int pthread_mutex_setprioceiling(pthread_mutex_t* mutex, int prioceiling)
{
    errno = ENOSYS;
    return -1;
}
