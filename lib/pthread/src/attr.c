#include <pthread.h>
#include <errno.h>

int pthread_attr_init(pthread_attr_t* attr)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_destroy(pthread_attr_t* attr)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_getdetachstate(const pthread_attr_t* attr, int* detachstate)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_setdetachstate(pthread_attr_t* attr, int detachstate)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_getguardsize(const pthread_attr_t* attr, size_t* guardsize)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_setguardsize(pthread_attr_t* attr, size_t guardsize)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_getinheritsched(const pthread_attr_t* attr, int* inheritsched)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_setinheritsched(pthread_attr_t* attr, int inheritsched)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_getschedparam(const pthread_attr_t* attr, struct sched_param* param)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_setschedparam(pthread_attr_t* attr, const struct sched_param* param)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_getschedpolicy(const pthread_attr_t* attr, int policy)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_setschedpolicy(pthread_attr_t* attr, int policy)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_getscope(const pthread_attr_t* attr, int* contentionscope)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_setscope(pthread_attr_t* attr, int contentionscope)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_getstackaddr(const pthread_attr_t* attr, void** stackaddr)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_setstackaddr(pthread_attr_t* attr, void* stackaddr)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_getstacksize(const pthread_attr_t* attr, size_t* stacksize)
{
    errno = ENOSYS;
    return -1;
}

int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize)
{
    errno = ENOSYS;
    return -1;
}
