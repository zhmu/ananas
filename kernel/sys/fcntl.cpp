/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/flags.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "syscall.h"

Result sys_fcntl(Thread* t, fdindex_t hindex, int cmd, const void* in, void* out)
{
    Process& process = t->t_process;

    /* Get the handle */
    FD* fd;
    if (auto result = syscall_get_fd(FD_TYPE_ANY, hindex, fd); result.IsFailure())
        return result;

    switch (cmd) {
        case F_DUPFD: {
            int min_fd = (int)(uintptr_t)in;
            FD* fd_out;
            fdindex_t idx_out = -1;
            if (auto result = fd::Clone(process, hindex, nullptr, process, fd_out, min_fd, idx_out);
                result.IsFailure())
                return result;
            return Result::Success(idx_out);
        }
        case F_GETFD:
            /* TODO */
            return Result::Success(0);
        case F_GETFL:
            /* TODO */
            return Result::Success(0);
        case F_SETFD: {
            return Result::Failure(EPERM);
        }
        case F_SETFL: {
            return Result::Failure(EPERM);
        }
        default:
            return Result::Failure(EINVAL);
    }

    // NOTREACHED
}
