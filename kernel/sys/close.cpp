/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "syscall.h"

Result sys_close(const fdindex_t index)
{
    FD* fd;
    if (auto result = syscall_get_fd(FD_TYPE_ANY, index, fd); result.IsFailure())
        return result;
    if (auto result = fd->Close(); result.IsFailure())
        return result;

    return Result::Success();
}
