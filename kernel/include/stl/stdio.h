/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int sprintf(char* str, const char* fmt, ...);
int snprintf(char* str, size_t len, const char* fmt, ...);

void vprintf(const char* fmt, va_list ap);
int vsnprintf(char* str, size_t len, const char* fmt, va_list ap);

#ifdef __cplusplus
} // extern "C"
#endif
