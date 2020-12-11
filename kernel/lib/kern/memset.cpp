/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/lib.h"

template<typename T>
size_t Fill(void*& d, size_t sz, T v)
{
    auto ptr = reinterpret_cast<T*>(d);
    for (uint64_t n = 0; n < sz / sizeof(T); ++n, ++ptr)
        *ptr = v;
    d = reinterpret_cast<void*>(ptr);
    return (sz / sizeof(T)) * sizeof(T);
}

void* memset(void* p, int c, size_t len)
{
    // Optimised by aligning to a 32-bit address and doing 32-bit operations
    // while possible
    auto dest = p;

    if (len >= 4 && (reinterpret_cast<uint64_t>(dest) & 3))
        len -= Fill(dest, len & 3, static_cast<uint8_t>(c));
    const uint32_t c32 = static_cast<uint32_t>(c) << 24 | static_cast<uint32_t>(c) << 16 |
                         static_cast<uint32_t>(c) << 8 | static_cast<uint32_t>(c);
    len -= Fill(dest, len, c32);
    Fill(dest, len, static_cast<uint8_t>(c));

    return p;
}
