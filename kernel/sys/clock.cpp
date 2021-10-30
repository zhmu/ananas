/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include <ananas/syscalls.h>
#include "kernel/result.h"
#include "kernel/time.h"

#include "kernel/lib.h"

Result sys_clock_settime(const int id, const struct timespec* tp)
{
    // XXX We don't support setting the time just yet
    return Result::Failure(EINVAL);
}

Result sys_clock_gettime(const int id, struct timespec* tp)
{
    switch (id) {
        case CLOCK_MONOTONIC: {
            *tp = time::GetTimeSinceBoot();
            return Result::Success();
        }
        case CLOCK_REALTIME: {
            *tp = time::GetTime();
            return Result::Success();
        }
        case CLOCK_SECONDS:
            break;
    }
    return Result::Failure(EINVAL);
}

Result sys_clock_getres(const int id, struct timespec* res)
{
    return Result::Failure(EINVAL);
}
