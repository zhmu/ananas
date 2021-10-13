/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/lib.h"

void* memchr(const void* s, int c, size_t n)
{
    auto ptr = static_cast<uint8_t*>(const_cast<void*>(s));
    for(; n > 0; --n, ++ptr) {
        if (*ptr == static_cast<uint8_t>(c))
            return static_cast<void*>(ptr);
    }
    return nullptr;
}
