/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

Result sys_close(Thread* t, fdindex_t index)
{
    TRACE(SYSCALL, FUNC, "t %p, fd %d", t, index);

    FD* fd;
    if (auto result = syscall_get_fd(*t, FD_TYPE_ANY, index, fd); result.IsFailure())
        return result;
    if (auto result = fd->Close(); result.IsFailure())
        return result;

    TRACE(SYSCALL, INFO, "t=%p, success", t);
    return Result::Success();
}
