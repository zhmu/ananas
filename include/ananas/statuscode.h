/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2020 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_STATUSCODE_H
#define ANANAS_STATUSCODE_H

#include <ananas/_types/statuscode.h>

// Extracting things from the statuscode_t provided by every systemcall
// on failure: bit 31 = set, and the error code is contained in the other bits
// on success: bit 31 = clear, and the result is contained in the other bits
static inline unsigned int ananas_statuscode_extract_errno(statuscode_t n) { return n & 0x7fffffff; }
static inline unsigned int ananas_statuscode_extract_value(statuscode_t n) { return n & 0x7fffffff; }

static inline statuscode_t
ananas_statuscode_make_failure(unsigned int no)
{
    return (1 << 31) | no;
}

static inline int ananas_statuscode_is_success(unsigned int val) { return (val & 0x80000000) == 0; }

static inline int ananas_statuscode_is_failure(unsigned int val)
{
    return !ananas_statuscode_is_success(val);
}

static inline statuscode_t ananas_statuscode_make_success(unsigned int val)
{
    return val & 0x7fffffff;
}

#endif /* ANANAS_STATUSCODE_H */
