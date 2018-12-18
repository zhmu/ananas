/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/lib.h"

/*
 * The naive implementation is easy but slow; we can do much better by
 * attempting to copy 32-bit chunks in a single go every time - this results in
 * a massive speedup (we choose 32-bit chunks because most CPU's seem to like
 * that)
 */
#undef NAIVE_IMPLEMENTATION

void* memset(void* b, int c, size_t len)
{
#ifdef NAIVE_IMPLEMENTATION
    char* ptr = (char*)b;
    while (len--) {
        *ptr++ = c;
    }
#else
    void* d = b;

    /* Sets sz bytes of variable type T-sized data */
#define DO_SET(T, sz, v)                                               \
    do {                                                               \
        size_t x = (size_t)sz;                                         \
        while (x >= sizeof(T)) {                                       \
            *(T*)d = (T)v;                                             \
            x -= sizeof(T);                                            \
            d = static_cast<void*>(static_cast<char*>(d) + sizeof(T)); \
            len -= sizeof(T);                                          \
        }                                                              \
    } while (0)

    /* First of all, attempt to align to 32-bit boundary */
    if (len >= 4 && ((addr_t)b & 3))
        DO_SET(uint8_t, len & 3, c);

    /* Attempt to set everything using 4 bytes at a time */
    DO_SET(uint32_t, len, ((uint32_t)c << 24 | (uint32_t)c << 16 | (uint32_t)c << 8 | (uint32_t)c));

    /* Handle the leftovers */
    DO_SET(uint8_t, len, c);

#undef DO_SET
#endif
    return b;
}
