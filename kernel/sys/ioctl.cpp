/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "syscall.h"

Result
sys_ioctl(const fdindex_t fdindex, const unsigned long req, void* arg1, void* arg2, void* arg3)
{
    FD* fd;
    if (auto result = syscall_get_fd(FD_TYPE_ANY, fdindex, fd); result.IsFailure())
        return result;

    if (fd->fd_ops->d_ioctl == nullptr)
        return Result::Failure(EINVAL);

    void* args[] = {arg1, arg2, arg3};
    return fd->fd_ops->d_ioctl(fdindex, *fd, req, args);
}
