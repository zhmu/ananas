/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/result.h"
#include "syscall.h"

Result sys_nanosleep(const struct timespec* rqtp, struct timespec* rmtp)
{
    return Result::Failure(EINVAL); // TODO
}
