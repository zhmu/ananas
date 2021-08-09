/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/lib.h"

void* memmove(void* dst, const void* src, size_t len)
{
    auto dst_c = static_cast<char*>(dst);
    auto src_c = static_cast<const char*>(src);

    if (dst_c <= src_c) {
        while(len--) {
            *dst_c++ = *src_c++;
        }
    } else {
        src_c += len;
        dst_c += len;
        while(len--) {
            *--dst_c = *--src_c;
        }
    }
    return dst;
}
