/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2020 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/thread.h"
#include "kernel-md/thread.h"

struct ThreadControlBlock {
    union {
        Thread thread;
        char stack[KERNEL_STACK_SIZE];
    } u;

    Thread* thread() { return u.thread; }
};
