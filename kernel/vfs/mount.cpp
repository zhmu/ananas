/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/icache.h"

namespace vfs
{
    namespace
    {
        const size_t Max_Mounted_FS = 16;
    } // unnamed namespace

    Spinlock spl_mountedfs;
    struct VFS_MOUNTED_FS mountedfs[Max_Mounted_FS];

    Spinlock spl_fstypes;
    VFSFileSystemList fstypes;

    size_t GetMaxMountedFilesystems() { return Max_Mounted_FS; }

} // namespace vfs

static struct VFS_MOUNTED_FS* vfs_get_availmountpoint()
{
    SpinlockGuard g(vfs::spl_mountedfs);
    for (unsigned int n = 0; n < vfs::Max_Mounted_FS; n++) {
        struct VFS_MOUNTED_FS* fs = &vfs::mountedfs[n];
        if ((fs->fs_flags & VFS_FLAG_INUSE) == 0) {
            fs->fs_flags |= VFS_FLAG_INUSE;
            return fs;
        }
    }
    return NULL;
}

Result vfs_mount(const char* from, const char* to, const char* type)
{
    /*
     * Attempt to locate the filesystem type; so we know what to call to mount
     * it.
     */
    struct VFS_FILESYSTEM_OPS* fsops = NULL;
    {
        SpinlockGuard g(vfs::spl_fstypes);
        for (auto& curfs : vfs::fstypes) {
            if (strcmp(curfs.fs_name, type) == 0) {
                /* Match */
                fsops = curfs.fs_fsops;
                break;
            }
        }
    }
    if (fsops == NULL)
        return Result::Failure(EINVAL);

    /* Locate the device to mount from */
    Device* dev = NULL;
    if (from != NULL) {
        dev = device_manager::FindDevice(from);
        if (dev == NULL)
            return Result::Failure(ENODEV);
    }

    /* Locate an available mountpoint and hook it up */
    struct VFS_MOUNTED_FS* fs = vfs_get_availmountpoint();
    if (fs == NULL)
        return Result::Failure(ENXIO); // XXX silly error
    fs->fs_device = dev;
    fs->fs_fsops = fsops;

    INode* root_inode = nullptr;
    if (auto result = fs->fs_fsops->mount(fs, root_inode); result.IsFailure()) {
        memset(fs, 0, sizeof(*fs));
        return result;
    }

    KASSERT(root_inode != NULL, "successful mount without a root inode");
    KASSERT(S_ISDIR(root_inode->i_sb.st_mode), "root inode isn't a directory");
    KASSERT(
        root_inode->i_refcount == 1, "bad refcount of root inode (must be 1, is %u)",
        root_inode->i_refcount);
    fs->fs_mountpoint = strdup(to);
    fs->fs_root_dentry = &dcache_create_root_dentry(fs);
    fs->fs_root_dentry->d_inode =
        root_inode; /* don't deref - we're giving the ref to the root dentry */

    /*
     * Override the root path dentry with our root inode; this effectively hooks
     * our filesystem to the parent. XXX I wonder if this is correct; we should
     * always just hook our path to the fs root dentry... need to think about it
     */
    DEntry* dentry_root = fs->fs_root_dentry;
    if (auto result = vfs_lookup(NULL, dentry_root, to);
        result.IsSuccess() && dentry_root != fs->fs_root_dentry) {
        if (dentry_root->d_inode != NULL)
            vfs_deref_inode(*dentry_root->d_inode);
        vfs_ref_inode(*root_inode);
        dentry_root->d_inode = root_inode;
    }

    return Result::Success();
}

void vfs_abandon_device(Device& device)
{
    SpinlockGuard g(vfs::spl_mountedfs);

    for (unsigned int n = 0; n < vfs::Max_Mounted_FS; n++) {
        struct VFS_MOUNTED_FS* fs = &vfs::mountedfs[n];
        if ((fs->fs_flags & VFS_FLAG_INUSE) == 0 || (fs->fs_flags & VFS_FLAG_ABANDONED) ||
            fs->fs_device != &device)
            continue;

        /*
         * This filesystem can no longer operate sanely - ensure all requests for it are
         * rejected instead of serviced.
         *
         * XXX We should at least start the unmount process here.
         */
        fs->fs_flags |= VFS_FLAG_ABANDONED;
        fs->fs_device = nullptr;
    }
}

Result vfs_unmount(const char* path)
{
    panic("fix me");

    SpinlockGuard g(vfs::spl_mountedfs);

    for (unsigned int n = 0; n < vfs::Max_Mounted_FS; n++) {
        struct VFS_MOUNTED_FS* fs = &vfs::mountedfs[n];
        if ((fs->fs_flags & VFS_FLAG_INUSE) && (fs->fs_mountpoint != NULL) &&
            (strcmp(fs->fs_mountpoint, path) == 0)) {
            /* Got it; disown it immediately. XXX What about pending inodes etc? */
            fs->fs_mountpoint = NULL;
            /* XXX Ask filesystem politely to unmount */
            fs->fs_flags = 0; /* Available */
            return Result::Success();
        }
    }
    return Result::Failure(ENOENT);
}

struct VFS_MOUNTED_FS* vfs_get_rootfs()
{
    SpinlockGuard g(vfs::spl_mountedfs);

    /* XXX the root fs should have a flag marking it as such */
    for (unsigned int n = 0; n < vfs::Max_Mounted_FS; n++) {
        struct VFS_MOUNTED_FS* fs = &vfs::mountedfs[n];
        if ((fs->fs_flags & VFS_FLAG_INUSE) && (fs->fs_mountpoint != NULL) &&
            (strcmp(fs->fs_mountpoint, "/") == 0))
            return fs;
    }
    return NULL;
}

Result vfs_register_filesystem(VFSFileSystem& fs)
{
    SpinlockGuard g(vfs::spl_fstypes);

    /* Ensure the filesystem is not already registered */
    for (auto& curfs : vfs::fstypes) {
        if (strcmp(curfs.fs_name, fs.fs_name) == 0) {
            /* Duplicate filesystem type; refuse to register */
            return Result::Failure(EEXIST);
        }
    }

    /* Filesystem is clear; hook it up */
    vfs::fstypes.push_back(fs);
    return Result::Success();
}

Result vfs_unregister_filesystem(VFSFileSystem& fs)
{
    SpinlockGuard g(vfs::spl_fstypes);

    vfs::fstypes.remove(fs);
    return Result::Success();
}

const kdb::RegisterCommand
    kdbMounts("mounts", "Show current mounts", [](int, const kdb::Argument*) {
        SpinlockGuard g(vfs::spl_fstypes);

        for (unsigned int n = 0; n < vfs::Max_Mounted_FS; n++) {
            struct VFS_MOUNTED_FS* fs = &vfs::mountedfs[n];
            if ((fs->fs_flags & VFS_FLAG_INUSE) == 0)
                continue;
            kprintf(
                ">> vfs=%p, flags=0x%x, mountpoint='%s'\n", fs, fs->fs_flags, fs->fs_mountpoint);
        }
    });
