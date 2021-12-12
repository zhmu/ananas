/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
using fdindex_t = int;
using dev_t = __dev_t;
using refcount_t = int;
using blocknr_t = __blocknr_t;
using off_t = __off_t;
using tick_t = __tick_t;
using pid_t = __pid_t;
using uid_t = __uid_t;
using gid_t = __gid_t;
using ino_t = __ino_t;
using socklen_t = __socklen_t;
using mode_t = __mode_t;
using time_t = __time_t;
using clockid_t = __clockid_t;
using key_t = __key_t;
#endif
