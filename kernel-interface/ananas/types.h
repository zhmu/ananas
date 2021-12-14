/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_TYPES_H__
#define __ANANAS_TYPES_H__

#ifndef ASM

#include <machine/_types.h>

typedef __uint64_t      __blkcnt_t;
typedef __uint64_t      __blksize_t;
typedef __uint64_t      __blocknr_t;
typedef __int32_t       __clock_t;
typedef int             __clockid_t;
typedef __uint32_t      __dev_t;
typedef __uint32_t      __gid_t;
typedef long            __id_t;
typedef __uint64_t      __ino_t;
typedef int             __key_t;
typedef int             __mode_t;
typedef __uint16_t      __nlink_t;
typedef __int64_t       __off_t;
typedef long            __pid_t;
typedef __SIZE_TYPE__   __socklen_t;
typedef __uint32_t      __statuscode_t;
typedef __uint32_t      __suseconds_t;
typedef __int32_t       __useconds_t;
typedef __uint64_t      __tick_t;
typedef __int64_t       __time_t;
typedef __uint32_t      __uid_t;

typedef __SIZE_TYPE__   size_t;

#ifdef KERNEL
typedef long            register_t;
typedef __uint64_t      addr_t;
#endif

#include <ananas/types/fdset.h>
#include <ananas/types/itimerval.h>
#include <ananas/types/sigset.h>
#include <ananas/types/timespec.h>
#include <ananas/types/timeval.h>
#include <ananas/types/stack.h>

/* __dev_t helpers */
#define __dev_t_major(d) ((d) >> 16)
#define __dev_t_minor(d) ((d)&0xffff)
#define __dev_t_make(ma, mi) (((ma) << 16) | ((mi)&0xffff))

#endif /* !ASM */

#endif /* __ANANAS_TYPES_H__ */
