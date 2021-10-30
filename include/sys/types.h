/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>

// XXX This is a hack to get libgcov to build (it doesn't include stdint.h)
#include <stdint.h>

typedef __blkcnt_t       blkcnt_t;
typedef __blksize_t      blksize_t;
typedef __blocknr_t      blocknr_t;
typedef __clock_t        clock_t;
typedef __clockid_t      clockid_t;
typedef __dev_t          dev_t;
typedef __gid_t          gid_t;
typedef __id_t           id_t;
typedef __ino_t          ino_t;
typedef __key_t          key_t;
typedef __mode_t         mode_t;
typedef __nlink_t        nlink_t;
typedef __off_t          off_t;
typedef __pid_t          pid_t;
typedef __socklen_t      socklen_t;
typedef __suseconds_t    suseconds_t;
typedef __tick_t         tick_t;
typedef __time_t         time_t;
typedef __uid_t          uid_t;

typedef __SIZE_TYPE__    size_t;
#define unsigned signed
typedef __SIZE_TYPE__    ssize_t;
#undef unsigned
