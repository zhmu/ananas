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

static inline struct pthread_mutexattr* get_mutexattr(pthread_mutexattr_t* attr)
{
    if (attr == NULL) {
        errno = EINVAL;
        return NULL;
    }
    return *attr;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr)
{
    struct pthread_mutexattr* ma = malloc(sizeof(struct pthread_mutexattr));
    if (ma == NULL)
        return -1;
    memset(ma, 0, sizeof(struct pthread_mutexattr));
    ma->ma_type = PTHREAD_MUTEX_DEFAULT;
    ma->ma_protocol = PTHREAD_PRIO_NONE;
    ma->ma_pshared = PTHREAD_PROCESS_PRIVATE;
    *attr = ma;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr)
{
    struct pthread_mutexattr* ma = get_mutexattr(attr);
    if (ma == NULL) return -1;
    free(ma);
    return 0;
}

int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t* attr, int* prioceiling)
{
    struct pthread_mutexattr* ma = get_mutexattr((pthread_mutexattr_t*)attr);
    if (ma == NULL) return -1;
    errno = ENOSYS;
    return -1;
}

int pthread_mutexattr_setprioceiling(pthread_mutexattr_t* attr, int prioceiling)
{
    struct pthread_mutexattr* ma = get_mutexattr(attr);
    if (ma == NULL) return -1;
    errno = ENOSYS;
    return -1;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t* attr, int* protocol)
{
    struct pthread_mutexattr* ma = get_mutexattr((pthread_mutexattr_t*)attr);
    if (ma == NULL) return -1;
    return ma->ma_protocol;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t* attr, int protocol)
{
    struct pthread_mutexattr* ma = get_mutexattr(attr);
    if (ma == NULL) return -1;
    errno = ENOSYS;
    return -1;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t* attr, int* pshared)
{
    struct pthread_mutexattr* ma = get_mutexattr((pthread_mutexattr_t*)attr);
    if (ma == NULL) return -1;
    return ma->ma_pshared;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t* attr, int psahred)
{
    struct pthread_mutexattr* ma = get_mutexattr(attr);
    if (ma == NULL) return -1;
    errno = ENOSYS;
    return -1;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type)
{
    struct pthread_mutexattr* ma = get_mutexattr((pthread_mutexattr_t*)attr);
    if (ma == NULL) return -1;
    return ma->ma_type;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type)
{
    struct pthread_mutexattr* ma = get_mutexattr(attr);
    if (ma == NULL) return -1;
    if (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_ERRORCHECK && type != PTHREAD_MUTEX_RECURSIVE) {
        errno = EINVAL;
        return -1;
    }
    ma->ma_type = type;
    return 0;
}
