/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <pthread.h>
#include <errno.h>

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr)
{
    errno = ENOSYS;
    return -1;
}

int pthread_cond_destroy(pthread_cond_t* cond)
{
    errno = ENOSYS;
    return -1;
}

int pthread_cond_broadcast(pthread_cond_t* cond)
{
    // errno = ENOSYS;
    // return -1;
    return 0;
}

int pthread_cond_signal(pthread_cond_t* cond)
{
    // errno = ENOSYS;
    // return -1;
    return 0;
}

int pthread_cond_timedwait(
    pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime)
{
    errno = ENOSYS;
    return -1;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
    errno = ENOSYS;
    return -1;
}
