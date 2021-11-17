/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <pthread.h>
#include <errno.h>

int pthread_condattr_init(pthread_condattr_t* attr)
{
    errno = ENOSYS;
    return -1;
}

int pthread_condattr_destroy(pthread_condattr_t* attr)
{
    errno = ENOSYS;
    return -1;
}

int pthread_condattr_getpshared(const pthread_condattr_t* attr, int* pshared)
{
    errno = ENOSYS;
    return -1;
}

int pthread_condattr_setpshared(pthread_condattr_t* attr, int pshared)
{
    errno = ENOSYS;
    return -1;
}
