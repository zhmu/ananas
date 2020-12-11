/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
/*
 * Ananas ext2fs filesystem driver.
 *
 * ext2 filesystems consist of a set of 'block groups', which have the following
 * format for block group 'g':
 *
 * - superblock (*)
 * - block group descriptor table (*)
 * - block bitmap
 * - inode bitmap
 * - inode table
 * - data blocks
 *
 * Fields marked with a (*) are only present if the SPARSE_BLOCKGROUP
 * filesystem flag is set and the blockgroup number is a power of
 * 2, 3, 5 or 7. They are always present if the flag is absent.
 *
 * The location of the first superblock is always fixed, at byte offset 1024.
 * The superblock 'sb' at this location will be used by ext2_mount(), where we
 * read all blockgroups. This allows us to locate any inode on the disk.
 *
 * The main reference material used is "The Second Extended File System:
 * Internal Layout" by Dave Poirier.
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/bio.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/mm.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/generic.h"
#include "kernel/vfs/mount.h"
#include "ext2.h"

template<typename Value> constexpr Value EXT2_TO_LE16(Value v) { return v; }
template<typename Value> constexpr Value EXT2_TO_LE32(Value v) { return v; }

namespace
{
    struct EXT2_FS_PRIVDATA {
        struct EXT2_SUPERBLOCK sb;

        unsigned int num_blockgroups;
        struct EXT2_BLOCKGROUP* blockgroup;
        unsigned int log_blocksize;
    };

    struct EXT2_INODE_PRIVDATA {
        uint32_t block[EXT2_INODE_BLOCKS];
    };

    static void ext2_conv_superblock(struct EXT2_SUPERBLOCK* sb)
    {
        /* XXX implement me */
        sb->s_magic = EXT2_TO_LE16(sb->s_magic);
    }

    static Result ext2_prepare_inode(INode& inode)
    {
        auto privdata = new EXT2_INODE_PRIVDATA;
        memset(privdata, 0, sizeof(struct EXT2_INODE_PRIVDATA));
        inode.i_privdata = privdata;
        return Result::Success();
    }

    static void ext2_discard_inode(INode& inode) { kfree(inode.i_privdata); }

#if 0
static void	
ext2_dump_inode(struct EXT2_INODE* inode)
{
	kprintf("mode    = 0x%x\n", inode->i_mode);
	kprintf("uid/gid = %u:%u\n", inode->i_uid, inode->i_gid);
	kprintf("size    = %u\n", inode->i_size);
	kprintf("blocks  =");
	for(int n = 0; n < 15; n++) {
		kprintf(" %u", inode->i_block[n]);
	}
	kprintf("\n");
}
#endif

    static bool
    ext2_determine_indirect(INode& inode, blocknr_t& block_in, int& level, blocknr_t& indirect)
    {
        auto& fs = *inode.i_fs;
        auto in_privdata = static_cast<struct EXT2_INODE_PRIVDATA*>(inode.i_privdata);
        auto bsOver4 = fs.fs_block_size / 4;
        block_in -= 12;
        if (block_in < bsOver4) {
            /* (b) first indirect */
            indirect = in_privdata->block[12];
            level = 0;
            return true;
        }

        block_in -= bsOver4;
        if (block_in < (bsOver4 * bsOver4)) {
            /* (c) double indirect */
            indirect = in_privdata->block[13];
            level = 1;
            return true;
        }

        block_in -= bsOver4 * bsOver4;
        if (block_in < bsOver4 * bsOver4 * (bsOver4 + 1)) {
            /* (d) triple indirect */
            indirect = in_privdata->block[14];
            level = 2;
            return true;
        }

        return false;
    }

    /*
     * Retrieves the disk block for a given file block. In ext2, the first 12 blocks
     * are direct blocks. Block 13 is the first indirect block and contains pointers to
     * blocks 13 - 13 + X - 1 (where X = blocksize / 4). Thus, for an 1KB block
     * size, blocks 13 - 268 can be located by reading the first indirect block.
     *
     * Block 14 contains the doubly-indirect block, which is a block-pointer to
     * another block in the same format as block 13. Block 14 contains X pointers,
     * and each block therein contains X pointers as well, so with a 1KB blocksize,
     * X * X = 65536 blocks can be stored.
     *
     * Block 15 is the triply-indirect block, which contains a block-pointer to
     * an doubly-indirect block. With an 1KB blocksize, each doubly-indirect block
     * contains X * X blocks, so we can store X * X * X = 16777216 blocks.
     */
    static Result
    ext2_block_map(INode& inode, blocknr_t block_in, blocknr_t& block_out, bool create)
    {
        struct VFS_MOUNTED_FS* fs = inode.i_fs;
        auto privdata = (struct EXT2_FS_PRIVDATA*)fs->fs_privdata;
        auto in_privdata = static_cast<struct EXT2_INODE_PRIVDATA*>(inode.i_privdata);
        auto orig_block_in = block_in;
        KASSERT(!create, "ext2: readonly filesystem");

        /*
         * We need to figure out whether we have to look up the block in the single,
         * double or triply-linked block. From the comments above, we know that:
         *
         * (a) The first 12 blocks (0 .. 11) can directly be accessed.
         * (b) The first indirect block contains blocks 12 .. (block_size / 4) + 11
         * (c) The double-indirect block contains blocks 12 + (block_size / 4) to
         *     (block_size / 4)^2 + (block_size / 4) + 11
         * (d) The triple-indirect block contains everything else.
         */

        /* (a) Direct blocks are easy */
        if (block_in < 12) {
            block_out = in_privdata->block[block_in];
            return Result::Success();
        }

        // Determine where to start and what to read
        int level;
        blocknr_t indirect;
        if (!ext2_determine_indirect(inode, block_in, level, indirect))
            return Result::Failure(ERANGE);

        /*
         * A 1KB block has spots for 1024/4 = 256 indirect block numbers; this
         * number needs to double for 2KB blocks, etc, so it is easiest just to
         * use the log2 of the blocksize. Note that log_blocksize is 0 for
         * 1024, so we just add 8 to it.
         *
         * This approach is inspired by GRUB's ext2fs code.
         */
        int block_shift = privdata->log_blocksize + 8;
        do {
            // Read the indirect block
            BIO* bio;
            if (auto result = vfs_bread(fs, indirect, &bio); result.IsFailure())
                return result;
            // Extract the next block to read
            indirect = [&]() {
                const auto blocks = reinterpret_cast<uint32_t*>(static_cast<char*>(bio->Data()));
                int blockIndex = (block_in >> (block_shift * level)) % (fs->fs_block_size / 4);
                return EXT2_TO_LE32(blocks[blockIndex]);
            }();
            bio->Release();
        } while (--level >= 0);

        block_out = indirect;

        return Result::Success();
    }

    static Result ext2_readdir(struct VFS_FILE* file, void* dirents, size_t len)
    {
        INode& inode = *file->f_dentry->d_inode;
        struct VFS_MOUNTED_FS* fs = inode.i_fs;
        blocknr_t blocknum = (blocknr_t)file->f_offset / (blocknr_t)fs->fs_block_size;
        uint32_t offset = file->f_offset % fs->fs_block_size;
        size_t written = 0, left = len;

        while (left > 0) {
            blocknr_t cur_block;
            if (auto result = ext2_block_map(inode, blocknum, cur_block, false); result.IsFailure())
                return result;
            if (cur_block == 0) {
                /*
                 * We've run out of blocks. Need to stop here.
                 */
                break;
            }

            BIO* bio;
            if (auto result = vfs_bread(fs, cur_block, &bio); result.IsFailure())
                return result;

            /*
             * ext2 directories contain variable-length records, which is why we have
             * this loop. Each record tells us how long it is. The 'offset'-part
             * always refers to the record we have to parse.
             *
             * We assume records will never cross a block, which is in line with the
             * specification.
             */
            auto ext2de =
                reinterpret_cast<struct EXT2_DIRENTRY*>(static_cast<char*>(bio->Data()) + offset);
            uint32_t inum = EXT2_TO_LE32(ext2de->inode);
            if (inum > 0) {
                /*
                 * Inode number values of zero indicate the entry is not used; this entry
                 * works and we mustreturn it.
                 */
                size_t filled = vfs_filldirent(
                    &dirents, left, inum, (const char*)ext2de->name, ext2de->name_len);
                if (!filled) {
                    /* out of space! */
                    bio->Release();
                    break;
                }
                written += filled;
                left -= filled;
            }

            /*
             * Update the offsets; this ensures we will read the correct block next
             * time.
             */
            file->f_offset += EXT2_TO_LE16(ext2de->rec_len);
            offset += EXT2_TO_LE16(ext2de->rec_len);
            if (offset >= fs->fs_block_size) {
                offset -= fs->fs_block_size;
                blocknum++;
            }

            bio->Release();
        }
        return Result::Success(written);
    }

    static struct VFS_INODE_OPS ext2_file_ops = {.block_map = ext2_block_map, .read = vfs_generic_read,};

    static struct VFS_INODE_OPS ext2_dir_ops = {.readdir = ext2_readdir,
                                                .lookup = vfs_generic_lookup};

    static Result ext2_read_link(INode& inode, char* buffer, size_t buflen)
    {
        auto in_privdata = static_cast<struct EXT2_INODE_PRIVDATA*>(inode.i_privdata);

        // XXX i_block[] may be byte-swapped - how to deal with this?
        size_t len = buflen;
        KASSERT(len > 0, "empty buffer?");
        buffer[len - 1] = '\0';
        if (len > 60)
            len = 60;
        memcpy(buffer, in_privdata->block, len);
        return Result::Success(len);
    }

    static struct VFS_INODE_OPS ext2_symlink_ops = {.read_link = ext2_read_link,
                                                    .follow_link = vfs_generic_follow_link};

    /*
     * Reads a filesystem inode and fills a corresponding inode structure.
     */
    static Result ext2_read_inode(INode& inode, ino_t inum)
    {
        struct VFS_MOUNTED_FS* fs = inode.i_fs;
        struct EXT2_FS_PRIVDATA* privdata = (struct EXT2_FS_PRIVDATA*)inode.i_fs->fs_privdata;

        /*
         * Inode number zero does not exists within ext2 (or Linux for that matter),
         * but it is considered wasteful to ignore an inode, so inode 1 maps to the
         * first inode entry on disk...
         */
        inum--;
        KASSERT(inum < privdata->sb.s_inodes_count, "inode out of range");

        /*
         * Every block group has a fixed number of inodes, so we can find the
         * blockgroup number and corresponding inode index within this blockgroup by
         * simple divide and modulo operations. These two are combined to figure out
         * the block we have to read.
         */
        uint32_t bgroup = inum / privdata->sb.s_inodes_per_group;
        uint32_t iindex = inum % privdata->sb.s_inodes_per_group;
        blocknr_t block = privdata->blockgroup[bgroup].bg_inode_table +
                          (iindex * privdata->sb.s_inode_size) / fs->fs_block_size;

        /* Fetch the block and make a pointer to the inode */
        BIO* bio;
        if (auto result = vfs_bread(fs, block, &bio); result.IsFailure())
            return result;
        unsigned int idx = (iindex * privdata->sb.s_inode_size) % fs->fs_block_size;
        auto ext2inode =
            reinterpret_cast<struct EXT2_INODE*>(static_cast<char*>(bio->Data()) + idx);

        /* Fill the stat buffer with date */
        inode.i_sb.st_ino = inum;
        inode.i_sb.st_mode = EXT2_TO_LE16(ext2inode->i_mode);
        inode.i_sb.st_nlink = EXT2_TO_LE16(ext2inode->i_links_count);
        inode.i_sb.st_uid = EXT2_TO_LE16(ext2inode->i_uid);
        inode.i_sb.st_gid = EXT2_TO_LE16(ext2inode->i_gid);
        inode.i_sb.st_atime = EXT2_TO_LE32(ext2inode->i_atime);
        inode.i_sb.st_mtime = EXT2_TO_LE32(ext2inode->i_mtime);
        inode.i_sb.st_ctime = EXT2_TO_LE32(ext2inode->i_ctime);
        inode.i_sb.st_blocks = EXT2_TO_LE32(ext2inode->i_blocks);
        inode.i_sb.st_size = EXT2_TO_LE32(ext2inode->i_size);

        /*
         * Copy the block pointers to the private inode structure; we need them for
         * any read/write operation.
         */
        auto iprivdata = static_cast<struct EXT2_INODE_PRIVDATA*>(inode.i_privdata);
        for (unsigned int i = 0; i < EXT2_INODE_BLOCKS; i++)
            iprivdata->block[i] = EXT2_TO_LE32(ext2inode->i_block[i]);

        /* Fill out the inode operations - this depends on the inode type */
        uint16_t imode = EXT2_TO_LE16(ext2inode->i_mode);
        switch (imode & 0xf000) {
            case EXT2_S_IFREG:
                inode.i_iops = &ext2_file_ops;
                break;
            case EXT2_S_IFDIR:
                inode.i_iops = &ext2_dir_ops;
                break;
            case EXT2_S_IFLNK:
                // XXX For now
                if (EXT2_TO_LE32(ext2inode->i_blocks) != 0) {
                    kprintf("ext2: symlink inode %d has blocks, unsupported yet!\n", (int)inum);
                    bio->Release();
                    return Result::Failure(EPERM);
                }
                inode.i_iops = &ext2_symlink_ops;
                break;
            default:
                bio->Release();
                return Result::Failure(EPERM);
        }
        bio->Release();

        return Result::Success();
    }

    static Result ext2_mount(struct VFS_MOUNTED_FS* fs, INode*& root_inode)
    {
        /* Default to 1KB blocksize and fetch the superblock */
        BIO* bio;
        fs->fs_block_size = 1024;
        if (auto result = vfs_bread(fs, 1, &bio); result.IsFailure())
            return result;

        /* See if something ext2-y lives here */
        auto sb = reinterpret_cast<struct EXT2_SUPERBLOCK*>(bio->Data());
        ext2_conv_superblock(sb);
        if (sb->s_magic != EXT2_SUPER_MAGIC) {
            bio->Release();
            return Result::Failure(ENODEV);
        }

        /* Fill out some fields with the defaults for very old ext2 filesystems */
        if (sb->s_rev_level == EXT2_GOOD_OLD_REV) {
            sb->s_inode_size = EXT2_GOOD_OLD_INODE_SIZE;
        }

        /* Victory */
        auto privdata = new EXT2_FS_PRIVDATA;
        memcpy(&privdata->sb, sb, sizeof(*sb));
        fs->fs_privdata = privdata;

        privdata->num_blockgroups =
            (sb->s_blocks_count - sb->s_first_data_block - 1) / sb->s_blocks_per_group + 1;
        privdata->blockgroup = new EXT2_BLOCKGROUP[privdata->num_blockgroups];
        privdata->log_blocksize = sb->s_log_block_size;

        /* Fill out filesystem fields */
        fs->fs_block_size = 1024L << sb->s_log_block_size;

        /* Free the superblock */
        bio->Release();

        /*
         * Read the block group descriptor table; we use the fact that this table
         * will always exist at the very first block group, and it will always appear
         * at the first data block.
         */
        for (unsigned int n = 0; n < privdata->num_blockgroups; n++) {
            /*
             * The +1 is because we need to skip the superblock, and the s_first_data_block
             * increment is because we need to count from the superblock onwards...
             */
            blocknr_t blocknum = 1 + (n * sizeof(struct EXT2_BLOCKGROUP)) / fs->fs_block_size;
            blocknum += privdata->sb.s_first_data_block;
            if (auto result = vfs_bread(fs, blocknum, &bio); result.IsFailure()) {
                kfree(privdata);
                return result;
            }
            memcpy(
                (void*)(privdata->blockgroup + n),
                (void*)(static_cast<char*>(bio->Data()) + ((n * sizeof(struct EXT2_BLOCKGROUP)) % fs->fs_block_size)),
                sizeof(struct EXT2_BLOCKGROUP));
            bio->Release();
        }

#if 0
	//for (unsigned int n = 0; n < privdata->num_blockgroups; n++) {
	for (unsigned int n = 19; n < 21; n++) {
		struct EXT2_BLOCKGROUP* bg = &privdata->blockgroup[n];
		kprintf("blockgroup %u: block=%u,inode=%u,table=%u,free_blocks=%u,free_inodes=%u,used_dirs=%u\n",
			n, bg->bg_block_bitmap, bg->bg_inode_bitmap, bg->bg_inode_table,
			bg->bg_free_blocks_count, bg->bg_free_inodes_count, bg->bg_used_dirs_count);
	}
#endif

        /* Read the root inode */
        if (auto result = vfs_get_inode(fs, EXT2_ROOT_INO, root_inode); result.IsFailure()) {
            kfree(privdata);
            return result;
        }
        return Result::Success();
    }

    struct VFS_FILESYSTEM_OPS fsops_ext2 = {.mount = ext2_mount,
                                            .prepare_inode = ext2_prepare_inode,
                                            .discard_inode = ext2_discard_inode,
                                            .read_inode = ext2_read_inode};

    VFSFileSystem fs_ext2("ext2", &fsops_ext2);

    const init::OnInit registerExt2FS(init::SubSystem::VFS, init::Order::Middle, []() {
        vfs_register_filesystem(fs_ext2);
    });

} // unnamed namespace
