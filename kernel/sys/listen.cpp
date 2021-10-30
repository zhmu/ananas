/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "syscall.h"

Result sys_listen(int socket, int backlog)
{
    FD* fd;
    if (auto result = syscall_get_fd(FD_TYPE_SOCKET, socket, fd); result.IsFailure())
        return result;

    if (fd->fd_ops->d_listen == nullptr)
        return Result::Failure(ENOTSOCK);
    return fd->fd_ops->d_listen(socket, *fd, backlog);
}
