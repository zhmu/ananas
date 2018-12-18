#include <errno.h>

int __cxa_thread_atexit_impl(void (*dtor_func)(void*), void* obj, void* dso_symbol)
{
    errno = ENOMEM;
    return -1;
}
