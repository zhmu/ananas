/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/generic.h"
#include "kernel/vfs/mount.h"
#include "device.h"
#include "filesystem.h"
#include "network.h"
#include "kernel.h"
#include "proc.h"
#include "root.h"
#include "support.h"

namespace ankhfs
{
    namespace
    {
        IAnkhSubSystem* subSystems[static_cast<size_t>(SubSystem::SS_Last)];

        IAnkhSubSystem* GetSubSystemFromInode(INode& inode)
        {
            auto subSystem = inum_to_subsystem(inode.i_inum);
            if (subSystem >= SS_Last)
                return nullptr;
            return subSystems[static_cast<int>(subSystem)];
        }

        Result ankhfs_read(struct VFS_FILE* file, void* buf, size_t len)
        {
            auto subSystem = GetSubSystemFromInode(*file->f_dentry->d_inode);
            if (subSystem == nullptr)
                return Result::Failure(EIO);
            return subSystem->HandleRead(file, buf, len);
        }

        Result ankhfs_ioctl(struct VFS_FILE* file, unsigned long op, void* args[])
        {
            auto subSystem = GetSubSystemFromInode(*file->f_dentry->d_inode);
            if (subSystem == nullptr)
                return Result::Failure(EIO);
            return subSystem->HandleIOControl(file, op, args);
        }

        Result ankhfs_readlink(INode& inode, char* buffer, size_t buflen)
        {
            auto subSystem = GetSubSystemFromInode(inode);
            if (subSystem == nullptr)
                return Result::Failure(EIO);
            return subSystem->HandleReadLink(inode, buffer, buflen);
        }

        Result ankhfs_open(VFS_FILE& file, Process* p)
        {
            auto subSystem = GetSubSystemFromInode(*file.f_dentry->d_inode);
            if (subSystem == nullptr)
                return Result::Failure(EIO);
            return subSystem->HandleOpen(file, p);
        }

        Result ankhfs_close(VFS_FILE& file, Process* p)
        {
            auto subSystem = GetSubSystemFromInode(*file.f_dentry->d_inode);
            if (subSystem == nullptr)
                return Result::Failure(EIO);
            return subSystem->HandleClose(file, p);
        }

        Result ankhfs_readdir(struct VFS_FILE* file, void* dirents, size_t len)
        {
            auto subSystem = GetSubSystemFromInode(*file->f_dentry->d_inode);
            if (subSystem == nullptr)
                return Result::Failure(EIO);
            return subSystem->HandleReadDir(file, dirents, len);
        }

        struct VFS_INODE_OPS ankhfs_no_ops = {};

        struct VFS_INODE_OPS ankhfs_file_ops = {.read = ankhfs_read, .ioctl = ankhfs_ioctl};

        struct VFS_INODE_OPS ankhfs_link_ops = {.read_link = ankhfs_readlink};

        struct VFS_INODE_OPS ankhfs_dev_ops = {
            .read = ankhfs_read, .open = ankhfs_open, .close = ankhfs_close};

        struct VFS_INODE_OPS ankhfs_dir_ops = {.readdir = ankhfs_readdir,
                                               .lookup = vfs_generic_lookup};

        Result ankhfs_mount(struct VFS_MOUNTED_FS* fs, INode*& root_inode)
        {
            fs->fs_block_size = 512;
            fs->fs_privdata = nullptr;

            subSystems[static_cast<size_t>(SubSystem::SS_Root)] = &GetRootSubSystem();
            subSystems[static_cast<size_t>(SubSystem::SS_Proc)] = &GetProcSubSystem();
            subSystems[static_cast<size_t>(SubSystem::SS_FileSystem)] = &GetFileSystemSubSystem();
            subSystems[static_cast<size_t>(SubSystem::SS_Device)] = &GetDeviceSubSystem();
            subSystems[static_cast<size_t>(SubSystem::SS_Kernel)] = &GetKernelSubSystem();
            subSystems[static_cast<size_t>(SubSystem::SS_Network)] = &GetNetworkSubSystem();

            auto result = vfs_get_inode(fs, make_inum(SS_Root, 0, 0), root_inode);
            KASSERT(
                result.IsSuccess(), "cannot get root inode of synthetic filesystem (%d)",
                result.AsStatusCode());
            return result;
        }

        Result ankhfs_read_inode(INode& inode, ino_t inum)
        {
            inode.i_sb.st_ino = inum;
            inode.i_sb.st_mode = 0444;
            inode.i_sb.st_nlink = 1;
            inode.i_sb.st_uid = 0;
            inode.i_sb.st_gid = 0;
            inode.i_sb.st_atime = 0; // XXX
            inode.i_sb.st_mtime = 0;
            inode.i_sb.st_ctime = 0;
            inode.i_sb.st_blocks = 0;
            inode.i_sb.st_size = 0;
            auto subSystem = GetSubSystemFromInode(inode);
            if (subSystem == nullptr)
                return Result::Failure(EIO);

            if (auto result = subSystem->FillInode(inode, inum); result.IsFailure())
                return result;

            if (S_ISDIR(inode.i_sb.st_mode)) {
                inode.i_sb.st_mode |= 0111;
                inode.i_iops = &ankhfs_dir_ops;
            } else if (S_ISREG(inode.i_sb.st_mode))
                inode.i_iops = &ankhfs_file_ops;
            else if (S_ISLNK(inode.i_sb.st_mode))
                inode.i_iops = &ankhfs_link_ops;
            else if (S_ISCHR(inode.i_sb.st_mode) || S_ISBLK(inode.i_sb.st_mode))
                inode.i_iops = &ankhfs_dev_ops;
            else
                inode.i_iops = &ankhfs_no_ops;

            return Result::Success();
        }

        struct VFS_FILESYSTEM_OPS fsops_ankhfs = {.mount = ankhfs_mount,
                                                  .read_inode = ankhfs_read_inode};

        struct VFSFileSystem fs_ankhfs("ankhfs", &fsops_ankhfs);

        const init::OnInit registerAnkhFS(init::SubSystem::VFS, init::Order::Middle, []() {
            vfs_register_filesystem(fs_ankhfs);
        });

    } // unnamed namespace
} // namespace ankhfs
