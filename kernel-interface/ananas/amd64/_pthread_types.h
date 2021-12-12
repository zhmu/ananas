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
    int m_type;
    int m_value;
    int m_num_recursive_locked;
};

struct pthread_mutexattr {
    int ma_type;
    int ma_protocol;
    int ma_pshared;
};

struct pthread_rwlock {
    struct pthread_mutex* rw_mutex;
};

struct pthread_rwlockattr {
    int rwla_pshared;
};

struct pthread {
    // Thread Specific Data
    void* tsd[PTHREAD_KEYS_MAX];
};

#endif // PTHREAD_AMD64_H
