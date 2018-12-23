/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef PTHREAD_AMD64_H
#define PTHREAD_AMD64_H

#include <ananas/limits.h>

struct pthread_attr {
    int unused;
};

struct pthread_cond {
    int unused;
};

struct pthread_condattr {
    int unused;
};

struct pthread_key {
    int unused;
};

struct pthread_mutex {
    int unused;
};

struct pthread_mutexattr {
    int unused;
};

struct pthread_once {
    int unused;
};

struct pthread_rwlock {
    int unused;
};

struct pthread_rwlockattr {
    int unused;
};

struct pthread {
    // Thread Specific Data
    void* tsd[PTHREAD_KEYS_MAX];
};

#endif // PTHREAD_AMD64_H
