/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include <sys/socket.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "syscall.h"

Result sys_send(int socket, const void* buf, size_t length, int flags)
{
    FD* fd;
    if (auto result = syscall_get_fd(FD_TYPE_SOCKET, socket, fd); result.IsFailure())
        return result;

    void* buffer;
    if (auto result = syscall_map_buffer(buf, length, vm::flag::Read, &buffer); result.IsFailure())
        return result;

    if (fd->fd_ops->d_send == nullptr)
        return Result::Failure(ENOTSOCK);
    return fd->fd_ops->d_send(socket, *fd, buffer, length, flags);
}
