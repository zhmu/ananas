/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/time.h"
#include "syscall.h"

Result sys_nanosleep(const struct timespec* rqtp, struct timespec* rmtp)
{
    if (rqtp == nullptr) Result::Failure(EINVAL);
    const auto deadline = time::GetTicks() + time::TimespecToTicks(*rqtp);

    thread::SleepUntilTick(deadline);
    if (rmtp != nullptr) *rmtp = {};
    return Result::Success();
}
