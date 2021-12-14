/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/result.h"
#include "syscall.h"

Result sys_getgroups(int count, gid_t* list)
{
    return Result::Failure(EINVAL);
}

Result sys_setgroups(int count, const gid_t* list)
{
    return Result::Failure(EINVAL);
}
