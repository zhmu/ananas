/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/bio.h"
#include "kernel/init.h"
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/vfs/types.h"
#include "kernel/vfs/mount.h"
#include "kernel/vfs/icache.h"

bool vfs_is_filesystem_sane(struct VFS_MOUNTED_FS* fs)
{
    SpinlockGuard g(fs->fs_spinlock);
    return (fs->fs_flags & VFS_FLAG_ABANDONED) == 0;
}

Result vfs_bread(struct VFS_MOUNTED_FS* fs, blocknr_t block, struct BIO** bio)
{
    if (!vfs_is_filesystem_sane(fs))
        return Result::Failure(EIO);

    BIO* bio2;
    if (auto result = bread(fs->fs_device, block * (fs->fs_block_size / BIO_SECTOR_SIZE), fs->fs_block_size, bio2); result.IsFailure())
        return result;
    *bio = bio2;
    return bio2->b_status;
}

size_t vfs_filldirent(void** dirents, size_t left, ino_t inum, const char* name, int namelen)
{
    /*
     * First of all, ensure we have sufficient space. Note that we use the fact
     * that de_name is only a single byte; we count it as the \0 byte.
     */
    int de_length = sizeof(struct VFS_DIRENT) + namelen;
    if (left < de_length)
        return 0;

    struct VFS_DIRENT* de = (struct VFS_DIRENT*)*dirents;
    *dirents = static_cast<void*>(static_cast<char*>(*dirents) + de_length);

    de->de_flags = 0; /* TODO */
    de->de_name_length = namelen;
    de->de_inum = inum;
    memcpy(de->de_name, name, namelen);
    de->de_name[namelen] = '\0'; /* zero terminate */

    /*
     * TODO Now that we have the entry, this is a good time to add it to our
     * dentry cache.
     */
    return de_length;
}

void vfs_set_inode_dirty(INode& inode)
{
    /* Ref the inode to prevent it from going away */
    vfs_ref_inode(inode);

    /* XXX For now, we just immediately write the inode */
    struct VFS_MOUNTED_FS* fs = inode.i_fs;
    fs->fs_fsops->write_inode(inode);

    vfs_deref_inode(inode);
}
