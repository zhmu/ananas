/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/syscalls.h>
#include <ananas/handle-options.h>
#include <ananas/errno.h>
#include <ananas/limits.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/thread.h"

Result sys_dup(Thread* t, fdindex_t index)
{
    Process& process = t->t_process;

    FD* fd_out;
    fdindex_t index_out;
    if (auto result = fd::Clone(process, index, nullptr, process, fd_out, 0, index_out);
        result.IsFailure())
        return result;

    return Result::Success(index_out);
}

Result sys_dup2(Thread* t, fdindex_t index, fdindex_t newindex)
{
    Process& process = t->t_process;
    if (newindex >= PROCESS_MAX_DESCRIPTORS)
        return Result::Failure(EINVAL);

    sys_close(t, newindex); // Ensure it is available; not an error if this fails

    FD* fd_out;
    fdindex_t index_out;
    if (auto result = fd::Clone(process, index, nullptr, process, fd_out, newindex, index_out);
        result.IsFailure())
        return result;

    return Result::Success(index_out);
}
