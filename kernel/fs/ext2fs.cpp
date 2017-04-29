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
#include <ananas/bio.h>
#include <ananas/error.h>
#include <ananas/vfs.h>
#include <ananas/vfs/generic.h>
#include <ananas/vfs/mount.h>
#include <ananas/init.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <ext2.h>

TRACE_SETUP;

#define EXT2_TO_LE16(x) (x)
#define EXT2_TO_LE32(x) (x)

struct EXT2_FS_PRIVDATA {
	struct EXT2_SUPERBLOCK sb;

	unsigned int num_blockgroups;
	struct EXT2_BLOCKGROUP* blockgroup;
};

struct EXT2_INODE_PRIVDATA {
	blocknr_t block[EXT2_INODE_BLOCKS];
};

static void
ext2_conv_superblock(struct EXT2_SUPERBLOCK* sb)
{
	/* XXX implement me */
	sb->s_magic = EXT2_TO_LE16(sb->s_magic);
}

static errorcode_t
ext2_prepare_inode(struct VFS_INODE* inode)
{
	auto privdata = new EXT2_INODE_PRIVDATA;
	memset(privdata, 0, sizeof(struct EXT2_INODE_PRIVDATA));
	inode->i_privdata = privdata;
	return ananas_success();
}

static void
ext2_discard_inode(struct VFS_INODE* inode)
{
	kfree(inode->i_privdata);
}

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
static errorcode_t
ext2_block_map(struct VFS_INODE* inode, blocknr_t block_in, blocknr_t* block_out, int create)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	auto in_privdata = static_cast<struct EXT2_INODE_PRIVDATA*>(inode->i_privdata);

	/*
	 * We need to figure out whether we have to look up the block in the single,
	 * double or triply-linked block. From the comments above, we know that:
	 *
	 * (a) The first 12 blocks (0 .. 11) can directly be accessed.
	 * (a) The first indirect block contains blocks 12 .. 12 + block_size / 4.
	 * (b) The double-indirect block contains blocks 12 + block_size / 4 to
	 *     13 + (block_size / 4)^2
	 * (c) The triple-indirect block contains everything else.
	 */

	/* (a) Direct blocks are easy */
	if (block_in < 12) {
		*block_out = in_privdata->block[block_in];
		return ananas_success();
	}

	if (block_in < 12 + fs->fs_block_size / 4) {
		/*
		 * (b) Need to look up the block the first indirect block, 13.
		 */
		struct BIO* bio;
		errorcode_t err = vfs_bread(fs, in_privdata->block[12], &bio);
		ANANAS_ERROR_RETURN(err);
		*block_out = EXT2_TO_LE32(*(uint32_t*)(static_cast<char*>(BIO_DATA(bio)) + (block_in - 12) * sizeof(uint32_t)));
		bio_free(bio);
		return ananas_success();
	}

	panic("ext2_block_map() needs support for doubly/triple indirect blocks!");
	return ANANAS_ERROR(BAD_RANGE);
}

static errorcode_t
ext2_readdir(struct VFS_FILE* file, void* dirents, size_t* len)
{
	struct VFS_INODE* inode = file->f_dentry->d_inode;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	blocknr_t blocknum = (blocknr_t)file->f_offset / (blocknr_t)fs->fs_block_size;
	uint32_t offset = file->f_offset % fs->fs_block_size;
	size_t written = 0, left = *len;

	struct BIO* bio = NULL;
	blocknr_t curblock = 0;
	while(left > 0) {
		blocknr_t block;
		errorcode_t err = ext2_block_map(inode, blocknum, &block, 0);
		ANANAS_ERROR_RETURN(err);
		if (block == 0) {
			/*
			 * We've run out of blocks. Need to stop here.
			 */
			break;
		}
		if(curblock != block) {
			if (bio != NULL) bio_free(bio);
			err = vfs_bread(fs, block, &bio);
			ANANAS_ERROR_RETURN(err);
			curblock = block;
		}

		/*
		 * ext2 directories contain variable-length records, which is why we have
		 * this loop. Each record tells us how long it is. The 'offset'-part
		 * always refers to the record we have to parse.
	 	 *
		 * We assume records will never cross a block, which is in line with the
		 * specification.
		 */
		auto ext2de = reinterpret_cast<struct EXT2_DIRENTRY*>(static_cast<char*>(BIO_DATA(bio)) + offset);
		uint32_t inum = EXT2_TO_LE32(ext2de->inode);
		if (inum > 0) {
			/*
			 * Inode number values of zero indicate the entry is not used; this entry
			 * works and we mustreturn it.
			 */
			int filled = vfs_filldirent(&dirents, &left, inum, (const char*)ext2de->name, ext2de->name_len);
			if (!filled) {
				/* out of space! */
				break;
			}
			written += filled;
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
	}
	if (bio != NULL) bio_free(bio);
	*len = written;
	return ananas_success();
}

static struct VFS_INODE_OPS ext2_file_ops = {
	.read = vfs_generic_read,
	.block_map = ext2_block_map
};

static struct VFS_INODE_OPS ext2_dir_ops = {
	.readdir = ext2_readdir,
	.lookup = vfs_generic_lookup
};

/*
 * Reads a filesystem inode and fills a corresponding inode structure.
 */
static errorcode_t
ext2_read_inode(struct VFS_INODE* inode, ino_t inum)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct EXT2_FS_PRIVDATA* privdata = (struct EXT2_FS_PRIVDATA*)inode->i_fs->fs_privdata;

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
	blocknr_t block = privdata->blockgroup[bgroup].bg_inode_table + (iindex * privdata->sb.s_inode_size) / fs->fs_block_size;

	/* Fetch the block and make a pointer to the inode */
	struct BIO* bio;
	errorcode_t err = vfs_bread(fs, block, &bio);
	ANANAS_ERROR_RETURN(err);
	unsigned int idx = (iindex * privdata->sb.s_inode_size) % fs->fs_block_size;
	auto ext2inode = reinterpret_cast<struct EXT2_INODE*>(static_cast<char*>(BIO_DATA(bio)) + idx);

	/* Fill the stat buffer with date */
	inode->i_sb.st_ino    = inum;
	inode->i_sb.st_mode   = EXT2_TO_LE16(ext2inode->i_mode);
	inode->i_sb.st_nlink  = EXT2_TO_LE16(ext2inode->i_links_count);
	inode->i_sb.st_uid    = EXT2_TO_LE16(ext2inode->i_uid);
	inode->i_sb.st_gid    = EXT2_TO_LE16(ext2inode->i_gid);
	inode->i_sb.st_atime  = EXT2_TO_LE32(ext2inode->i_atime);
	inode->i_sb.st_mtime  = EXT2_TO_LE32(ext2inode->i_mtime);
	inode->i_sb.st_ctime  = EXT2_TO_LE32(ext2inode->i_ctime);
	inode->i_sb.st_blocks = EXT2_TO_LE32(ext2inode->i_blocks);
	inode->i_sb.st_size   = EXT2_TO_LE32(ext2inode->i_size);

	/*
	 * Copy the block pointers to the private inode structure; we need them for
	 * any read/write operation.
	 */
	auto iprivdata = static_cast<struct EXT2_INODE_PRIVDATA*>(inode->i_privdata);
	for (unsigned int i = 0; i < EXT2_INODE_BLOCKS; i++)
		iprivdata->block[i] = EXT2_TO_LE32(ext2inode->i_block[i]);

	/* Fill out the inode operations - this depends on the inode type */
	uint16_t imode = EXT2_TO_LE16(ext2inode->i_mode);
	switch(imode & 0xf000) {
		case EXT2_S_IFREG:
			inode->i_iops = &ext2_file_ops;
			break;
		case EXT2_S_IFDIR:
			inode->i_iops = &ext2_dir_ops;
			break;
	}
	bio_free(bio);

	return ananas_success();
}

static errorcode_t
ext2_mount(struct VFS_MOUNTED_FS* fs, struct VFS_INODE** root_inode)
{
	/* Default to 1KB blocksize and fetch the superblock */
	struct BIO* bio;
	fs->fs_block_size = 1024;
	errorcode_t err = vfs_bread(fs, 1, &bio);
	ANANAS_ERROR_RETURN(err);

	/* See if something ext2-y lives here */
	struct EXT2_SUPERBLOCK* sb = (struct EXT2_SUPERBLOCK*)BIO_DATA(bio);
	ext2_conv_superblock(sb);
	if (sb->s_magic != EXT2_SUPER_MAGIC) {
		bio_free(bio);
		return ANANAS_ERROR(NO_DEVICE);
	}

	/* Fill out some fields with the defaults for very old ext2 filesystems */
	if (sb->s_rev_level == EXT2_GOOD_OLD_REV) {
		sb->s_inode_size = EXT2_GOOD_OLD_INODE_SIZE;
	}

	/* Victory */
	auto privdata = new EXT2_FS_PRIVDATA;
	memcpy(&privdata->sb, sb, sizeof(*sb));
	fs->fs_privdata = privdata;

	privdata->num_blockgroups = (sb->s_blocks_count - sb->s_first_data_block - 1) / sb->s_blocks_per_group + 1;
	privdata->blockgroup = new EXT2_BLOCKGROUP[privdata->num_blockgroups];

	/* Fill out filesystem fields */
	fs->fs_block_size = 1024L << sb->s_log_block_size;

	/* Free the superblock */
	bio_free(bio);

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
		err = vfs_bread(fs, blocknum, &bio);
		if (ananas_is_failure(err)) {
			kfree(privdata);
			return err;
		}
		memcpy((void*)(privdata->blockgroup + n),
		       (void*)(static_cast<char*>(BIO_DATA(bio)) + ((n * sizeof(struct EXT2_BLOCKGROUP)) % fs->fs_block_size)),
		       sizeof(struct EXT2_BLOCKGROUP));
		bio_free(bio);
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
	err = vfs_get_inode(fs, EXT2_ROOT_INO, root_inode);
	if (ananas_is_failure(err)) {
		kfree(privdata);
		return err;
	}
	return ananas_success();
}

static struct VFS_FILESYSTEM_OPS fsops_ext2 = {
	.mount = ext2_mount,
	.prepare_inode = ext2_prepare_inode,
	.discard_inode = ext2_discard_inode,
	.read_inode = ext2_read_inode
};

static struct VFS_FILESYSTEM fs_ext2 = {
	.fs_name = "ext2",
	.fs_fsops = &fsops_ext2
};

errorcode_t
ext2_init()
{
	return vfs_register_filesystem(&fs_ext2);
}

static errorcode_t
ext2_exit()
{
	return vfs_unregister_filesystem(&fs_ext2);
}

INIT_FUNCTION(ext2_init, SUBSYSTEM_VFS, ORDER_MIDDLE);
EXIT_FUNCTION(ext2_exit);

/* vim:set ts=2 sw=2: */
