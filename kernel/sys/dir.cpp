/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

namespace
{
    Result SetCWD(Process& proc, struct VFS_FILE& file)
    {
        DEntry& cwd = *proc.p_cwd;
        DEntry& new_cwd = *file.f_dentry;

        dentry_ref(new_cwd);
        proc.p_cwd = &new_cwd;
        dentry_deref(cwd);
        return Result::Success();
    }

} // unnamed namespace

Result sys_chdir(Thread* t, const char* path)
{
    Process& proc = *t->t_process;
    DEntry* cwd = proc.p_cwd;

    struct VFS_FILE file;
    if (auto result = vfs_open(&proc, path, cwd, &file); result.IsFailure())
        return result;

    /* XXX Check if file has a directory */

    // Open succeeded - now update the cwd
    auto result = SetCWD(proc, file);
    vfs_close(&proc, &file);
    return result;
}

Result sys_fchdir(Thread* t, fdindex_t index)
{
    Process& proc = *t->t_process;

    FD* fd;
    if (auto result = syscall_get_fd(*t, FD_TYPE_FILE, index, fd); result.IsFailure())
        return result;

    return SetCWD(proc, fd->fd_data.d_vfs_file);
}

Result sys_getcwd(Thread* t, char* buf, size_t size)
{
    Process& proc = *t->t_process;
    DEntry& cwd = *proc.p_cwd;

    if (size == 0)
        return Result::Failure(EINVAL);

    size_t pathLen = dentry_construct_path(NULL, 0, cwd);
    if (size < pathLen + 1)
        return Result::Failure(ERANGE);

    dentry_construct_path(buf, size, cwd);
    return Result::Success();
}
