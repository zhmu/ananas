/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/process.h"
#include "kernel/processgroup.h"
#include "syscall.h"

Result sys_getrlimit(int resource, struct rlimit* rlim)
{
    return Result::Failure(EINVAL);
}

Result sys_setrlimit(int resource, const struct rlimit* rlim)
{
    return Result::Failure(EINVAL);
}

Result sys_getpriority(int which, id_t who)
{
    return Result::Failure(EINVAL);
}

Result sys_setpriority(int which, id_t who, int value)
{
    return Result::Failure(EINVAL);
}
