/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "kernel-md/md.h"

Result syscall_get_fd(int type, fdindex_t index, FD*& fd)
{
    auto& t = thread::GetCurrent();
    return fd::Lookup(t.t_process, index, type, fd);
}

Result syscall_get_file(fdindex_t index, struct VFS_FILE** out)
{
    auto& t = thread::GetCurrent();
    FD* fd;
    if (auto result = fd::Lookup(t.t_process, index, FD_TYPE_FILE, fd); result.IsFailure())
        return result;

    struct VFS_FILE* file = &fd->fd_data.d_vfs_file;
    if (file->f_dentry == NULL && file->f_device == NULL)
        return Result::Failure(EBADF);

    *out = file;
    return Result::Success();
}

Result syscall_map_string(const void* ptr, const char** out)
{
    auto& t = thread::GetCurrent();
    auto x = static_cast<const char*>(
        md::thread::MapThreadMemory(t, (void*)ptr, PAGE_SIZE, VM_FLAG_READ));
    if (x == NULL)
        return Result::Failure(EFAULT);

    /* ensure it's zero terminated */
    for (int i = 0; i < PAGE_SIZE /* XXX */; i++)
        if (x[i] == '\0') {
            *out = x;
            return Result::Success();
        }

    return Result::Failure(EFAULT);
}

Result syscall_map_buffer(const void* ptr, size_t len, int flags, void** out)
{
    auto& t = thread::GetCurrent();
    void* x = md::thread::MapThreadMemory(t, (void*)ptr, len, flags);
    if (x == NULL)
        return Result::Failure(EFAULT);

    *out = x;
    return Result::Success();
}

Result syscall_set_handleindex(fdindex_t* ptr, fdindex_t index)
{
    auto& t = thread::GetCurrent();
    auto p = static_cast<fdindex_t*>(
        md::thread::MapThreadMemory(t, (void*)ptr, sizeof(fdindex_t), VM_FLAG_WRITE));
    if (p == NULL)
        return Result::Failure(EFAULT);

    *p = index;
    return Result::Success();
}

Result syscall_fetch_offset(const void* ptr, off_t* out)
{
    auto& t = thread::GetCurrent();
    auto o = static_cast<off_t*>(
        md::thread::MapThreadMemory(t, (void*)ptr, sizeof(off_t), VM_FLAG_READ));
    if (o == NULL)
        return Result::Failure(EFAULT);

    *out = *o;
    return Result::Success();
}

Result syscall_set_offset(void* ptr, off_t len)
{
    auto& t = thread::GetCurrent();
    auto o = static_cast<size_t*>(
        md::thread::MapThreadMemory(t, (void*)ptr, sizeof(size_t), VM_FLAG_WRITE));
    if (o == NULL)
        return Result::Failure(EFAULT);

    *o = len;
    return Result::Success();
}
