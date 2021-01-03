/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __MACHINE_TYPES_H__
#define __MACHINE_TYPES_H__

typedef char __int8_t;
typedef unsigned char __uint8_t;
typedef short __int16_t;
typedef unsigned short __uint16_t;
typedef int __int32_t;
typedef unsigned int __uint32_t;
typedef long __int64_t;
typedef unsigned long __uint64_t;

typedef __int64_t __intptr_t;
typedef __uint64_t __uintptr_t;

typedef __int64_t __intmax_t;
typedef __uint64_t __uintmax_t;

typedef __uint64_t __addr_t;
typedef __int64_t __ssize_t;
typedef __int64_t __register_t;

typedef __int64_t __time_t;
typedef __int32_t __clock_t;

#endif /* __MACHINE_TYPES_H__ */
