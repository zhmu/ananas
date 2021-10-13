/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

void* memchr(const void* s, int c, size_t n);
void* memcpy(void* dst, const void* src, size_t len) __nonnull;
void* memmove(void* dst, const void* src, size_t len);
void* memset(void* b, int c, size_t len) __nonnull;
int memcmp(const void* s1, const void* s2, size_t len) __nonnull;

char* strdup(const char* s) __nonnull;
char* strcpy(char* dst, const char* src) __nonnull;
char* strncpy(char* dst, const char* src, size_t n) __nonnull;
int strcmp(const char* s1, const char* s2) __nonnull;
int strncmp(const char* s1, const char* s2, size_t n) __nonnull;
char* strchr(const char* s, int c) __nonnull;
char* strrchr(const char* s, int c) __nonnull;
size_t strlen(const char* s) __nonnull;
char* strcat(char* dst, const char* src) __nonnull;
#ifdef __cplusplus
}
#endif
