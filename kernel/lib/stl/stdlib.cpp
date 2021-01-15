/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2020 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lib.h"

// Implementation of functionality normally present in libc's stdlib; intended
// to be called by libstdc++-v3
extern "C"
{
    void abort()
    {
        panic("abort");
    }
}
