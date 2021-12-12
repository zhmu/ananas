/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include <ananas/socket.h>
#include "kernel/fd.h"
#include "kernel/vm.h"
#include "kernel/result.h"
#include "syscall.h"

Result sys_accept(int socket, struct sockaddr* address, socklen_t* address_len)
{
    FD* fd;
    if (auto result = syscall_get_fd(FD_TYPE_SOCKET, socket, fd); result.IsFailure())
        return result;

    socklen_t* addr_len = nullptr;
    void* buffer = nullptr;
    if (address != nullptr) {
        void* addr_buf;
        if (auto result = syscall_map_buffer(address_len, sizeof(socklen_t), vm::flag::Read | vm::flag::Write, &addr_buf); result.IsFailure())
            return result;
        addr_len = static_cast<socklen_t*>(addr_buf);

        if (*addr_len != sizeof(struct sockaddr))
            return Result::Failure(EINVAL);

        if (auto result = syscall_map_buffer(address, *addr_len, vm::flag::Write, &buffer); result.IsFailure())
            return result;
    }

    if (fd->fd_ops->d_accept == nullptr)
        return Result::Failure(ENOTSOCK);
    return fd->fd_ops->d_accept(socket, *fd, static_cast<struct sockaddr*>(buffer), addr_len);
}
