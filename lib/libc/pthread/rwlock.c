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
#include "internal.h"

static inline struct pthread_rwlock* get_rwlock(pthread_rwlock_t* rwl)
{
    if (rwl == NULL) {
        errno = EINVAL;
        return NULL;
    }
    return *rwl;
}

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr)
{
    struct pthread_rwlock* rwl = malloc(sizeof(struct pthread_rwlock));
    if (rwl == NULL) return -1;

    // XXX We completely ignore attr for now

    memset(rwl, 0, sizeof(struct pthread_rwlock));
    if (pthread_mutex_init(&rwl->rw_mutex, NULL) < 0) {
        free(rwl); // XXX hope this does not set errno
        return -1;
    }

    *rwlock = rwl;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock)
{
    struct pthread_rwlock* rwl = get_rwlock(rwlock);
    if (rwl == NULL) return -1;

    pthread_mutex_destroy(&rwl->rw_mutex);
    free(rwl);
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock)
{
    struct pthread_rwlock* rwl = get_rwlock(rwlock);
    if (rwl == NULL) return -1;
    return pthread_mutex_lock(&rwl->rw_mutex);
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock)
{
    struct pthread_rwlock* rwl = get_rwlock(rwlock);
    if (rwl == NULL) return -1;
    return pthread_mutex_trylock(&rwl->rw_mutex);
}

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock)
{
    return pthread_rwlock_tryrdlock(rwlock);
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock)
{
    struct pthread_rwlock* rwl = get_rwlock(rwlock);
    if (rwl == NULL) return -1;
    return pthread_mutex_unlock(&rwl->rw_mutex);
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock)
{
    return pthread_rwlock_rdlock(rwlock);
}
