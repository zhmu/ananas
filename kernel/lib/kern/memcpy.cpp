/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lib.h"
#include <cstdint>

template<typename T>
size_t Copy(void*& d, const void*& s, size_t sz)
{
    auto dst = reinterpret_cast<T*>(d);
    auto src = reinterpret_cast<const T*>(s);
    for (uint64_t n = 0; n < sz / sizeof(T); ++n)
        *dst++ = *src++;
    d = reinterpret_cast<void*>(dst);
    s = reinterpret_cast<const void*>(src);
    return (sz / sizeof(T)) * sizeof(T);
}

void* memcpy(void* dst, const void* src, size_t len)
{
    auto ret = dst;

    // Optimised by aligning to a 32-bit address and doing 32-bit operations
    // while possible
    if (len >= 4 && (reinterpret_cast<uint64_t>(dst) & 3))
        len -= Copy<uint8_t>(dst, src, len & 3);
    len -= Copy<uint32_t>(dst, src, len);
    Copy<uint8_t>(dst, src, len);
    return ret;
}
