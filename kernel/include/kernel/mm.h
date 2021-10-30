/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <cstddef>
#include "kernel/cdefs.h"

#ifdef __cplusplus
#include <new>

extern "C" {
#endif
void* kmalloc(size_t len) __malloc;
void kfree(void* ptr);
#ifdef __cplusplus
}
#endif


