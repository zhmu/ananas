/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/syscalls.h>
#include "kernel/result.h"
#include "kernel/time.h"

#include "kernel/lib.h"

Result sys_clock_settime(Thread* t, int id, const struct timespec* tp)
{
    // XXX We don't support setting the time just yet
    return RESULT_MAKE_FAILURE(EINVAL);
}

Result sys_clock_gettime(Thread* t, int id, struct timespec* tp)
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
    return RESULT_MAKE_FAILURE(EINVAL);
}

Result sys_clock_getres(Thread* t, int id, struct timespec* res)
{
    return RESULT_MAKE_FAILURE(EINVAL);
}
