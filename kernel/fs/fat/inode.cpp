/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/bio.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/vfs/types.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/generic.h"
#include "block.h"
#include "dir.h"
#include "inode.h"
#include "fat.h"
#include "fatfs.h"

Result fat_prepare_inode(INode& inode)
{
    inode.i_privdata = new FAT_INODE_PRIVDATA;
    memset(inode.i_privdata, 0, sizeof(struct FAT_INODE_PRIVDATA));
    return Result::Success();
}

void fat_discard_inode(INode& inode)
{
    auto privdata = static_cast<struct FAT_INODE_PRIVDATA*>(inode.i_privdata);

    /*
     * If the file has backing storage, we need to throw away all inode's cluster
     * items in the cache.
     */
    if (privdata->first_cluster != 0)
        fat_clear_cache(inode.i_fs, privdata->first_cluster);

    kfree(inode.i_privdata);
}

static void fat_fill_inode(INode& inode, ino_t inum, struct FAT_ENTRY* fentry)
{
    struct VFS_MOUNTED_FS* fs = inode.i_fs;
    auto privdata = static_cast<struct FAT_INODE_PRIVDATA*>(inode.i_privdata);
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);

    inode.i_sb.st_ino = inum;
    inode.i_sb.st_mode = 0755;
    inode.i_sb.st_nlink = 1;
    inode.i_sb.st_uid = 0;
    inode.i_sb.st_gid = 0;
    /* TODO */
    inode.i_sb.st_atime = 0;
    inode.i_sb.st_mtime = 0;
    inode.i_sb.st_ctime = 0;
    if (fentry != NULL) {
        inode.i_sb.st_size = FAT_FROM_LE32(fentry->fe_size);
        uint32_t cluster = FAT_FROM_LE16(fentry->fe_cluster_lo);
        if (fs_privdata->fat_type == 32)
            cluster |= FAT_FROM_LE16(fentry->fe_cluster_hi) << 16;
        privdata->first_cluster = cluster;
        /* Distinguish between directory and inode */
        if (fentry->fe_attributes & FAT_ATTRIBUTE_DIRECTORY) {
            inode.i_iops = &fat_dir_ops;
            inode.i_sb.st_mode |= S_IFDIR;
        } else {
            inode.i_iops = &fat_inode_ops;
            inode.i_sb.st_mode |= S_IFREG;
        }
    } else {
        /* Root inode */
        inode.i_sb.st_size = fs_privdata->num_rootdir_sectors * fs_privdata->sector_size;
        inode.i_iops = &fat_dir_ops;
        inode.i_sb.st_mode |= S_IFDIR;
        if (fs_privdata->fat_type == 32)
            privdata->first_cluster = fs_privdata->first_rootdir_sector;
    }
    inode.i_sb.st_blocks = inode.i_sb.st_size / fs_privdata->sector_size;
    if (inode.i_sb.st_size % fs_privdata->sector_size)
        inode.i_sb.st_blocks++;
}

Result fat_read_inode(INode& inode, ino_t inum)
{
    struct VFS_MOUNTED_FS* fs = inode.i_fs;
    auto privdata = static_cast<struct FAT_INODE_PRIVDATA*>(inode.i_privdata);

    /*
     * FAT doesn't really have a root inode, so we just fake one. The root_inode
     * flag makes the read/write functions do the right thing.
     */
    if (inum == FAT_ROOTINODE_INUM) {
        privdata->root_inode = 1;
        fat_fill_inode(inode, inum, NULL);
        return Result::Success();
    }

    /*
     * For ordinary inodes, the inum is just the block with the 16-bit offset
     * within that block.
     */
    uint32_t block = inum >> 16;
    uint32_t offset = inum & 0xffff;
    KASSERT(
        offset <= fs->fs_block_size - sizeof(struct FAT_ENTRY), "inode offset %u out of range",
        offset);
    BIO* bio;
    if (auto result = vfs_bread(fs, block, &bio); result.IsFailure())
        return result;

    /* Fill out the inode details */
    auto fentry = reinterpret_cast<struct FAT_ENTRY*>(static_cast<char*>(bio->Data()) + offset);
    fat_fill_inode(inode, inum, fentry);
    bio->Release();
    return Result::Success();
}

Result fat_write_inode(INode& inode)
{
    struct VFS_MOUNTED_FS* fs = inode.i_fs;
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);
    auto privdata = static_cast<struct FAT_INODE_PRIVDATA*>(inode.i_privdata);
    ino_t inum = inode.i_inum;
    KASSERT(inum != FAT_ROOTINODE_INUM, "writing the root inode");

    /* Grab the current inode data block */
    uint32_t block = inum >> 16;
    uint32_t offset = inum & 0xffff;
    KASSERT(
        offset <= fs->fs_block_size - sizeof(struct FAT_ENTRY), "inode offset %u out of range",
        offset);
    BIO* bio;
    if (auto result = vfs_bread(fs, block, &bio); result.IsFailure())
        return result;

    /* Fill out the inode details */
    auto fentry = reinterpret_cast<struct FAT_ENTRY*>(static_cast<char*>(bio->Data()) + offset);
    FAT_TO_LE32(fentry->fe_size, inode.i_sb.st_size);
    FAT_TO_LE16(fentry->fe_cluster_lo, privdata->first_cluster & 0xffff);
    if (fs_privdata->fat_type == 32)
        FAT_TO_LE16(fentry->fe_cluster_hi, privdata->first_cluster >> 16);

    /* If the inode has run out of references, we must free the clusters it occupied */
    if (inode.i_sb.st_nlink == 0) {
        if (auto result = fat_truncate_clusterchain(inode); result.IsFailure()) {
            bio->Release();
            return result; // XXX Should this be fatal?
        }
    }

    /* And off it goes */
    return bio->Write();
}

struct VFS_INODE_OPS fat_inode_ops = {
    .read = vfs_generic_read, .write = vfs_generic_write, .block_map = fat_block_map};
