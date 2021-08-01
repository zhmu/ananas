/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/flags.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/process.h"

Result sys_open(const char* path, int flags, int mode)
{
    auto& proc = process::GetCurrent();

    /* Obtain a new handle */
    FD* fd;
    fdindex_t index_out;
    if (auto result = fd::Allocate(FD_TYPE_FILE, proc, 0, fd, index_out); result.IsFailure())
        return result;

    /*
     * Ask the handle to open the resource - if there isn't an open operation, we
     * assume this handle type cannot be opened using a syscall.
     */
    Result result = Result::Failure(EINVAL);
    if (fd->fd_ops->d_open != NULL)
        result = fd->fd_ops->d_open(index_out, *fd, path, flags, mode);

    if (result.IsFailure()) {
        fd->Close();
        return result;
    }
    return Result::Success(index_out);
}
