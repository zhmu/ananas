/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/generic.h"
#include "kernel/vfs/icache.h"

Result vfs_generic_lookup(DEntry& parent, INode*& destinode, const char* dentry)
{
    char buf[1024]; /* XXX */

    /*
     * XXX This is a very naive implementation which does not use the
     * possible directory index.
     */
    INode& parent_inode = *parent.d_inode;
    KASSERT(S_ISDIR(parent_inode.i_sb.st_mode), "supplied inode is not a directory");

    /* Rewind the directory back; we'll be traversing it from front to back */
    struct VFS_FILE dirf;
    memset(&dirf, 0, sizeof(dirf));
    dirf.f_offset = 0;
    dirf.f_dentry = &parent;
    while (1) {
        if (!vfs_is_filesystem_sane(parent_inode.i_fs))
            return Result::Failure(EIO);

        auto result = vfs_read(&dirf, buf, sizeof(buf));
        if (result.IsFailure())
            return result;
        auto buf_len = result.AsValue();
        if (buf_len == 0)
            return Result::Failure(ENOENT);

        char* cur_ptr = buf;
        while (buf_len > 0) {
            struct VFS_DIRENT* de = (struct VFS_DIRENT*)cur_ptr;
            buf_len -= DE_LENGTH(de);
            cur_ptr += DE_LENGTH(de);

#ifdef DEBUG_VFS_LOOKUP
            kprintf("vfs_generic_lookup('%s'): comparing with '%s'\n", dentry, de->de_name);
#endif

            if (strcmp(de->de_name, dentry) != 0)
                continue;

            /* Found it! */
            return vfs_get_inode(parent_inode.i_fs, de->de_inum, destinode);
        }
    }
}

Result vfs_generic_read(struct VFS_FILE* file, void* buf, size_t len)
{
    INode& inode = *file->f_dentry->d_inode;
    struct VFS_MOUNTED_FS* fs = inode.i_fs;
    size_t read = 0;
    size_t left = len;

    KASSERT(inode.i_iops->block_map != NULL, "called without block_map implementation");

    /* Adjust left so that we don't attempt to read beyond the end of the file */
    if ((inode.i_sb.st_size - file->f_offset) < left) {
        left = inode.i_sb.st_size - file->f_offset;
    }

    while (left > 0) {
        if (!vfs_is_filesystem_sane(inode.i_fs))
            return Result::Failure(EIO);

        /* Figure out which block to use next */
        blocknr_t cur_block;
        if (auto result = inode.i_iops->block_map(
                inode, (file->f_offset / (blocknr_t)fs->fs_block_size), cur_block, false);
            result.IsFailure())
            return result;

        // Grab the block
        BIO* bio;
        if (auto result = vfs_bread(fs, cur_block, &bio); result.IsFailure())
            return result;

        /* Copy as much from the current entry as we can */
        off_t cur_offset = file->f_offset % (blocknr_t)fs->fs_block_size;
        int chunk_len = fs->fs_block_size - cur_offset;
        if (chunk_len > left)
            chunk_len = left;
        KASSERT(chunk_len > 0, "attempt to handle empty chunk");
        memcpy(buf, (void*)(static_cast<char*>(bio->Data()) + cur_offset), chunk_len);
        bio->Release();

        read += chunk_len;
        buf = static_cast<void*>(static_cast<char*>(buf) + chunk_len);
        left -= chunk_len;
        file->f_offset += chunk_len;
    }
    return Result::Success(read);
}

Result vfs_generic_write(struct VFS_FILE* file, const void* buf, size_t len)
{
    INode& inode = *file->f_dentry->d_inode;
    struct VFS_MOUNTED_FS* fs = inode.i_fs;
    size_t written = 0;
    size_t left = len;
    BIO* bio = nullptr;

    KASSERT(inode.i_iops->block_map != NULL, "called without block_map implementation");

    int inode_dirty = 0;
    while (left > 0) {
        blocknr_t logical_block = file->f_offset / (blocknr_t)fs->fs_block_size;
        bool create =
            logical_block >= inode.i_sb.st_blocks; // XXX is this correct with sparse files?

        if (!vfs_is_filesystem_sane(inode.i_fs))
            return Result::Failure(EIO);

        // Figure out which block to use next
        blocknr_t cur_block;
        if (auto result = inode.i_iops->block_map(inode, logical_block, cur_block, create);
            result.IsFailure())
            return result;

        /* Calculate how much we have to put in the block */
        off_t cur_offset = file->f_offset % (blocknr_t)fs->fs_block_size;
        int chunk_len = fs->fs_block_size - cur_offset;
        if (chunk_len > left)
            chunk_len = left;

        // Grab the next block
        /* TODO Only read the block if it's a new one or we're not replacing everything
         * If (create || chunk_len == fs->fs_block_size), we don't _need_ the data... */
        if (auto result = vfs_bread(fs, cur_block, &bio); result.IsFailure())
            return result;

        /* Copy as much to the block as we can */
        KASSERT(chunk_len > 0, "attempt to handle empty chunk");
        memcpy((void*)(static_cast<char*>(bio->Data()) + cur_offset), buf, chunk_len);
        bio->Write();

        /* Update the offsets and sizes */
        written += chunk_len;
        buf = static_cast<const void*>(static_cast<const char*>(buf) + chunk_len);
        left -= chunk_len;
        file->f_offset += chunk_len;

        /*
         * If we had to create a new block or we'd have to write beyond the current
         * inode's size, enlarge the inode and mark it as dirty.
         */
        if (create || file->f_offset > inode.i_sb.st_size) {
            inode.i_sb.st_size = file->f_offset;
            inode_dirty++;
        }
    }

    if (inode_dirty)
        vfs_set_inode_dirty(inode);
    return Result::Success(written);
}

Result vfs_generic_follow_link(INode& inode, DEntry& base, DEntry*& followed)
{
    struct VFS_MOUNTED_FS* fs = inode.i_fs;

    char buf[1024]; // XXX
    auto result = inode.i_iops->read_link(inode, buf, sizeof(buf));
    if (result.IsFailure())
        return result;
    auto buflen = result.AsValue();
    if (buflen > 0)
        buf[buflen - 1] = '\0';

    return vfs_lookup(base.d_parent, followed, buf);
}
