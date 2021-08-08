/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// XXX We need to re-think this once we have actual threads...
static struct pthread main_pthread;

int pthread_create(
    pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine)(void*), void* arg)
{
    errno = ENOSYS;
    return -1;
}

int pthread_detach(pthread_t thread)
{
    errno = ENOSYS;
    return -1;
}

int pthread_equal(pthread_t t1, pthread_t t2) { return t1 == t2; }

void pthread_exit(void* value_ptr) { exit(0); }

int pthread_join(pthread_t thread, void** value_ptr)
{
    errno = ENOSYS;
    return -1;
}

pthread_t pthread_self(void) { return &main_pthread; }
