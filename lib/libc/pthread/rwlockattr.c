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

static inline struct pthread_rwlockattr* get_attr(pthread_rwlockattr_t* attr)
{
    if (attr == NULL) {
        errno = EINVAL;
        return NULL;
    }
    return *attr;
}

int pthread_rwlockattr_init(pthread_rwlockattr_t* attr)
{
    struct pthread_rwlockattr* rwla = malloc(sizeof(struct pthread_rwlockattr));
    if (rwla == NULL)
        return -1;

    memset(rwla, 0, sizeof(struct pthread_rwlockattr));
    rwla->rwla_pshared = PTHREAD_PROCESS_PRIVATE;
    *attr = rwla;
    return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t* attr)
{
    struct pthread_rwlockattr* rwla = get_attr(attr);
    if (rwla == NULL) return -1;
    free(rwla);
    return 0;
}

int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t* attr, int* pshared)
{
    struct pthread_rwlockattr* rwla = get_attr((pthread_rwlockattr_t*)attr);
    if (rwla == NULL) return -1;
    return rwla->rwla_pshared;
}

int pthread_rwlockattr_setpshared(pthread_rwlockattr_t* attr, int pshared)
{
    struct pthread_rwlockattr* rwla = get_attr(attr);
    if (rwla == NULL) return -1;
    errno = ENOSYS;
    return -1;
}
