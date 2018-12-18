#include <pthread.h>
#include <errno.h>

void* pthread_getspecific(pthread_key_t key) { return NULL; }

int pthread_setspecific(pthread_key_t key, const void* value)
{
    errno = ENOSYS;
    return -1;
}

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
    errno = ENOSYS;
    return -1;
}

int pthread_key_delete(pthread_key_t key)
{
    errno = ENOSYS;
    return -1;
}
