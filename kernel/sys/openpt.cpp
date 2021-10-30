/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include "kernel/result.h"
#include "kernel/dev/pseudotty.h"
#include "kernel/process.h"
#include "kernel/fd.h"
#include "syscall.h"

#include "kernel/lib.h"

Result sys_openpt(int flags)
{
    auto& proc = process::GetCurrent();

    /* Obtain a new handle */
    FD* fd;
    fdindex_t index_out;
    if (auto result = fd::Allocate(FD_TYPE_FILE, proc, 0, fd, index_out); result.IsFailure())
        return result;

    auto pseudoTTY = pseudotty::AllocatePseudoTTY();
    fd->fd_data.d_vfs_file.f_device = pseudoTTY->p_master; // XXX refcount
    return Result::Success(index_out);
}
