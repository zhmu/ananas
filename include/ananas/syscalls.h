/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscall-vmops.h>
#include <ananas/stat.h>
#include <ananas/_types/socklen.h>
#include <ananas/_types/fdset.h>

struct utimbuf;
struct Thread;
struct sigaction;

#ifdef KERNEL
class Result;
#else
typedef statuscode_t Result;
#endif

#include <_gen/syscalls.h>
