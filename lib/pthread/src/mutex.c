/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "internal.h"

#include <stdio.h>

#define MUTEX_VALUE_DESTROYED -1
#define MUTEX_VALUE_AVAILABLE 0
#define MUTEX_VALUE_LOCKED 1

static __attribute__((noreturn)) void die(const char* s)
{
    write(STDERR_FILENO, s, strlen(s));
    abort();
}

static const struct pthread_mutexattr default_mutex_attr = {
    PTHREAD_MUTEX_DEFAULT,
    PTHREAD_PRIO_NONE,
    PTHREAD_PROCESS_PRIVATE
};

static inline void init_mutex(struct pthread_mutex* m, const struct pthread_mutexattr* ma)
{
    memset(m, 0, sizeof(struct pthread_mutex));
    m->m_type = ma->ma_type;
    m->m_value = MUTEX_VALUE_AVAILABLE;
}

static inline struct pthread_mutex* get_mutex(pthread_mutex_t* mutex)
{
    if (mutex == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (*mutex == PTHREAD_MUTEX_INITIALIZER) {
        struct pthread_mutex* m = malloc(sizeof(struct pthread_mutex));
        if (m == NULL) return NULL;
        init_mutex(m, &default_mutex_attr);
        *mutex = m;
    }
    return *mutex;
}

#define ACQUIRE_LOCK_SUCCESS 0
#define ACQUIRE_LOCK_ALREADY_LOCKED -1

static inline int try_acquire(struct pthread_mutex* m)
{
    int old_value = __sync_val_compare_and_swap(&m->m_value, MUTEX_VALUE_AVAILABLE, MUTEX_VALUE_LOCKED);
    switch(old_value) {
        case MUTEX_VALUE_AVAILABLE:
            if (m->m_type == PTHREAD_MUTEX_RECURSIVE)
                __sync_add_and_fetch(&m->m_num_recursive_locked, 1);
            return ACQUIRE_LOCK_SUCCESS;
        case MUTEX_VALUE_LOCKED:
            if (m->m_type != PTHREAD_MUTEX_RECURSIVE)
                return ACQUIRE_LOCK_ALREADY_LOCKED;
            __sync_add_and_fetch(&m->m_num_recursive_locked, 1);
            return ACQUIRE_LOCK_SUCCESS;
        default:
            die("try_acquire on corrupt mutex");
    }
}

#define UNLOCK_SUCCESS 0
#define UNLOCK_NOT_LOCKED -1

static inline int try_unlock(struct pthread_mutex* m)
{
    if (m->m_type == PTHREAD_MUTEX_RECURSIVE) {
        if (__sync_sub_and_fetch(&m->m_num_recursive_locked, 1) != 0)
            return UNLOCK_SUCCESS;

        // FALLTHROUGH to normal mutex
    }

    return __sync_bool_compare_and_swap(&m->m_value, MUTEX_VALUE_LOCKED, MUTEX_VALUE_AVAILABLE) ? UNLOCK_SUCCESS : UNLOCK_NOT_LOCKED;
}


int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr)
{
    struct pthread_mutex* m = malloc(sizeof(struct pthread_mutex));
    if (m == NULL) return -1;
    init_mutex(m, attr != NULL ? *attr : &default_mutex_attr);
    *mutex = m;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    struct pthread_mutex* m = get_mutex(mutex);
    if (m == NULL) return -1;

    int old_value = __sync_val_compare_and_swap(&m->m_value, MUTEX_VALUE_AVAILABLE, MUTEX_VALUE_DESTROYED);
    switch(old_value) {
        case MUTEX_VALUE_AVAILABLE:
            break;
        case MUTEX_VALUE_LOCKED:
            die("pthread_mutex_destroy on locked mutex");
        default:
            die("pthread_mutex_destroy on corrupt mutex");
    }
    m->m_value = MUTEX_VALUE_DESTROYED;
    free(m);
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    struct pthread_mutex* m = get_mutex(mutex);
    if (m == NULL) return -1;
    assert(m->m_type != MUTEX_VALUE_DESTROYED);

    for(;;) {
        if (try_acquire(m) == ACQUIRE_LOCK_SUCCESS)
            return 0;

        // Need to sleep; this needs kernel support TODO
        die("pthread_mutex_lock need to sleep");
    }

    // NOTREACHED
}

int pthread_mutex_trylock(pthread_mutex_t* mutex)
{
    struct pthread_mutex* m = get_mutex(mutex);
    if (m == NULL) return -1;
    assert(m->m_type != MUTEX_VALUE_DESTROYED);

    if (try_acquire(m) == ACQUIRE_LOCK_ALREADY_LOCKED) {
        errno = EBUSY;
        return -1;
    }
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
    struct pthread_mutex* m = get_mutex(mutex);
    if (m == NULL) return -1;
    assert(m->m_type != MUTEX_VALUE_DESTROYED);

    if (try_unlock(m) != UNLOCK_SUCCESS)
        die("pthread_mutex_unlock on unlocked mutex");
    return 0;
}

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
