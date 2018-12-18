/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <sys/mount.h>
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/mount.h"
#include "kernel/device.h"
#include "kernel/fd.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/lock.h"
#include "syscall.h"

TRACE_SETUP;

namespace vfs
{
    extern Spinlock spl_mountedfs;
    extern struct VFS_MOUNTED_FS mountedfs[];
    size_t GetMaxMountedFilesystems();

} // namespace vfs

namespace
{
    struct VFS_MOUNTED_FS* FindFS(const char* path)
    {
        // XXX This is not correct; the first filesystem we find may not be the most accurate
        size_t path_len = strlen(path);
        for (unsigned int n = 0; n < vfs::GetMaxMountedFilesystems(); n++) {
            auto& fs = vfs::mountedfs[n];
            if (strncmp(fs.fs_mountpoint, path, path_len) == 0) {
                if (fs.fs_mountpoint[path_len] == '\0' || fs.fs_mountpoint[path_len] == '/')
                    return &fs;
            }
        }
        return nullptr;
    }

    void FillStat(struct VFS_MOUNTED_FS& vfs, struct statfs* buf)
    {
        memset(buf, 0, sizeof(struct statfs));
        buf->f_bsize = vfs.fs_block_size;
        if (vfs.fs_flags & VFS_FLAG_READONLY)
            buf->f_flags |= MNT_RDONLY;
        strncpy(buf->f_mntonname, vfs.fs_mountpoint, MNAMELEN);
        if (vfs.fs_device != nullptr) {
            const auto& device = *vfs.fs_device;
            snprintf(buf->f_mntfromname, MNAMELEN, "%s%d", device.d_Name, device.d_Unit);
        }
        // XXX fill out the rest of the fields
    }

} // unnamed namespace

Result sys_statfs(Thread* t, const char* path, struct statfs* buf)
{
    TRACE(SYSCALL, FUNC, "t=%p, path='%s', buf=%p", t, path, buf);

    // XXX we should realpath path() first - or let userland deal with that?

    SpinlockGuard g(vfs::spl_mountedfs);
    auto fs = FindFS(path);
    if (fs == nullptr)
        return RESULT_MAKE_FAILURE(ENOENT);

    FillStat(*fs, buf);
    return Result::Success();
}

Result sys_fstatfs(Thread* t, fdindex_t hindex, struct statfs* buf)
{
    TRACE(SYSCALL, FUNC, "t=%p, hindex=%d, buf=%p", t, hindex, buf);

    /* Get the handle */
    FD* fd;
    if (auto result = syscall_get_fd(*t, FD_TYPE_FILE, hindex, fd); result.IsFailure())
        return result;

    auto* dentry = fd->fd_data.d_vfs_file.f_dentry;
    if (dentry == nullptr)
        return RESULT_MAKE_FAILURE(EBADF);

    FillStat(*dentry->d_fs, buf);
    return Result::Success();
}

Result sys_mount(Thread* t, const char* type, const char* source, const char* dir, int flags)
{
    TRACE(SYSCALL, FUNC, "t=%p, type=%s source=%s dir=%s flags=%d", t, type, source, dir, flags);

    return vfs_mount(source, dir, type);
}

Result sys_unmount(Thread* t, const char* dir, int flags)
{
    TRACE(SYSCALL, FUNC, "t=%p, dir=%s flags=%d", t, dir, flags);

    return vfs_unmount(dir);
}
