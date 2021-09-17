/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/_types/socklen.h>
#include <sys/socket.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "syscall.h"

Result sys_connect(int socket, const struct sockaddr* address, socklen_t address_len)
{
    FD* fd;
    if (auto result = syscall_get_fd(FD_TYPE_SOCKET, socket, fd); result.IsFailure())
        return result;

    void* buffer;
    if (auto result = syscall_map_buffer(address, address_len, vm::flag::Read, &buffer); result.IsFailure())
        return result;

    if (fd->fd_ops->d_connect == nullptr)
        return Result::Failure(ENOTSOCK);
    return fd->fd_ops->d_connect(socket, *fd, static_cast<struct sockaddr*>(buffer), address_len);
}
