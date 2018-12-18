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
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

Result sys_write(Thread* t, fdindex_t hindex, const void* buf, size_t len)
{
    TRACE(SYSCALL, FUNC, "t=%p, hindex=%u, buf=%p, len=%d", t, hindex, buf, len);

    FD* fd;
    if (auto result = syscall_get_fd(*t, FD_TYPE_ANY, hindex, fd); result.IsFailure())
        return result;

    // Attempt to map the buffer readonly
    void* buffer;
    if (auto result = syscall_map_buffer(*t, buf, len, VM_FLAG_READ, &buffer); result.IsFailure())
        return result;

    // And write data from to it
    if (fd->fd_ops->d_write == NULL)
        return RESULT_MAKE_FAILURE(EINVAL);
    auto result = fd->fd_ops->d_write(t, hindex, *fd, buf, len);
    if (result.IsFailure())
        return result;

    TRACE(SYSCALL, FUNC, "t=%p, success: size=%d", t, result.AsStatusCode());
    return result;
}
