/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <pthread.h>
#include <errno.h>

int pthread_setconcurrency(int new_level)
{
    errno = ENOSYS;
    return -1;
}

int pthread_getconcurrency(void)
{
    errno = ENOSYS;
    return -1;
}
