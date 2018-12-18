/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "lib.h"

void* memcpy(void* dest, const void* src, size_t len)
{
    char* d = (char*)dest;
    char* s = (char*)src;
    while (len--) {
        *d++ = *s++;
    }
    return dest;
}

void* memset(void* b, int c, size_t len)
{
    char* ptr = (char*)b;
    while (len--) {
        *ptr++ = c;
    }
    return b;
}
