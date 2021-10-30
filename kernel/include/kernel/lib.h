/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/cdefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KASSERT(x, msg, args...) \
    if (!(x))                    \
    _panic(__FILE__, __func__, __LINE__, msg, ##args)

#define panic(msg, args...) _panic(__FILE__, __func__, __LINE__, msg, ##args)

/* Rounds up 'n' to a multiple of 'mult' */
#define ROUND_UP(n, mult) (((n) % (mult) == 0) ? (n) : (n) + ((mult) - (n) % (mult)))

/* Rounds down 'n' to a multiple of 'mult' */
#define ROUND_DOWN(n, mult) (((n) % (mult) == 0) ? (n) : (n) - (n) % (mult))

#ifdef __cplusplus
// Placement new declaration
void* operator new(size_t, void*) throw();

extern "C" {
#endif

void kprintf(const char* fmt, ...);
[[noreturn]] void _panic(const char* file, const char* func, int line, const char* fmt, ...);

#ifdef __cplusplus
} // extern "C"
#endif
