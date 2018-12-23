/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <pthread.h>
#include <errno.h>
#include "internal.h"

static volatile void* keys[PTHREAD_KEYS_MAX] = {0};
static void dummy_destructor() {}

void* pthread_getspecific(pthread_key_t key)
{
    if (key >= PTHREAD_KEYS_MAX) {
        return NULL;
    }
    return pthread_self()->tsd[key];
}

int pthread_setspecific(pthread_key_t key, const void* value)
{
    if (key >= PTHREAD_KEYS_MAX) {
        errno = EINVAL;
        return -1;
    }
    pthread_self()->tsd[key] = (void*)value;
    return 0;
}

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
    if (destructor == NULL)
        destructor = dummy_destructor;

    for (int n = 0; n < PTHREAD_KEYS_MAX; n++) {
        if (!__sync_bool_compare_and_swap(&keys[n], NULL, destructor))
            continue;
        *key = n;
        return 0;
    }
    errno = EAGAIN;
    return -1;
}

int pthread_key_delete(pthread_key_t key)
{
    if (key >= PTHREAD_KEYS_MAX) {
        errno = EINVAL;
        return -1;
    }
    keys[key] = NULL;
    return 0;
}
