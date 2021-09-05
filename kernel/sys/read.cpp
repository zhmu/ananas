/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "syscall.h"

Result sys_read(const fdindex_t hindex, void* buf, const size_t len)
{
    FD* fd;
    if (auto result = syscall_get_fd(FD_TYPE_ANY, hindex, fd); result.IsFailure())
        return result;

    // Attempt to map the buffer write-only
    void* buffer;
    if (auto result = syscall_map_buffer(buf, len, vm::flag::Write, &buffer); result.IsFailure())
        return result;

    // And read data to it
    if (fd->fd_ops->d_read == nullptr)
        return Result::Failure(EINVAL);

    return fd->fd_ops->d_read(hindex, *fd, buf, len);
}
