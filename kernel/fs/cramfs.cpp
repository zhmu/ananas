/*
 * Ananas cramfs filesystem driver.
 *
 * cramfs is a very simple, space-efficient filesystem designed for ramdisks
 * (there is no reason why it couldn't be backed on ROM, but what's in a
 * name).
 *
 * The filesystem consists of three distinct parts, in order:
 * - superblock (contains the root directory)
 * - inodes
 * - file data
 *
 * An important observation is that all fields in cramfs are 32-bit aligned;
 * the result is that all offsets obtained must be multiplied by 4.
 *
 * Directories are real easy: the inode's offset points to the first inode
 * present in the directory, and the length of the directory inode is used
 * to determine how much inodes have to be read. An inode contains the number
 * of bytes that follow it (the name length), which contain the entry name.
 *
 * Files are less trivial because they are compressed. Every file is split
 * into chunks of CRAMFS_PAGE_SIZE (4KB), and these chunks are compressed
 * one by one. There are a total of '(size - 1 / CRAMFS_PAGE_SIZE) + 1' of
 * such chunks (numchunks).
 *
 * Before the compressed data begins, there is 'numchunks' times a 32-bit
 * value which contains the offset of the next chunk's compressed data
 * (this makes sense, as the first chunk is always right after this list)
 */
#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/bio.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/generic.h"
#include "kernel/vfs/mount.h"
#include "../lib/zlib/zlib.h"
#include "cramfs.h"

TRACE_SETUP;

#define CRAMFS_DEBUG_READDIR(x...)

#define CRAMFS_PAGE_SIZE 4096

#define CRAMFS_TO_LE16(x) (x)
#define CRAMFS_TO_LE32(x) (x)

static z_stream cramfs_zstream;

struct CRAMFS_INODE_PRIVDATA {
	uint32_t offset;
};

struct CRAMFS_PRIVDATA {
	unsigned char temp_buf[CRAMFS_PAGE_SIZE * 2];
	unsigned char decompress_buf[CRAMFS_PAGE_SIZE + 4];
};

static errorcode_t
cramfs_read(struct VFS_FILE* file, void* buf, size_t* len)
{
	struct VFS_INODE* inode = file->f_dentry->d_inode;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct CRAMFS_PRIVDATA* fs_privdata = (struct CRAMFS_PRIVDATA*)fs->fs_privdata;
	struct CRAMFS_INODE_PRIVDATA* i_privdata = (struct CRAMFS_INODE_PRIVDATA*)inode->i_privdata;

	size_t total = 0, toread = *len;
	while (toread > 0) {
		uint32_t page_index = file->f_offset / CRAMFS_PAGE_SIZE;
		int cur_block = -1;
		struct BIO* bio;

		/* Calculate the compressed data offset of this page */
		cur_block = (i_privdata->offset + page_index * sizeof(uint32_t)) / fs->fs_block_size;
		errorcode_t err = vfs_bread(fs, cur_block, &bio);
		ANANAS_ERROR_RETURN(err);
		uint32_t next_offset = *(uint32_t*)(static_cast<char*>(BIO_DATA(bio)) + (i_privdata->offset + page_index * sizeof(uint32_t)) % fs->fs_block_size);
		bio_free(bio);

		uint32_t start_offset = 0;
		if (page_index > 0) {
			/* Now, fetch the offset of the previous page; this gives us the length of the compressed chunk */
			int prev_block = (i_privdata->offset + (page_index - 1) * sizeof(uint32_t)) / fs->fs_block_size;
			if (cur_block != prev_block) {
				errorcode_t err = vfs_bread(fs, prev_block, &bio);
				ANANAS_ERROR_RETURN(err);
			}
			start_offset = *(uint32_t*)(static_cast<char*>(BIO_DATA(bio)) + (i_privdata->offset + (page_index - 1) * sizeof(uint32_t)) % fs->fs_block_size);
		} else {
			/* In case of the first page, we have to set the offset ourselves as there is no index we can use */
			start_offset  = i_privdata->offset;
			start_offset += (((inode->i_sb.st_size - 1) / CRAMFS_PAGE_SIZE) + 1) * sizeof(uint32_t);
		}

		uint32_t left = next_offset - start_offset;
		KASSERT(left < sizeof(fs_privdata->temp_buf), "chunk too large");

		uint32_t buf_pos = 0;
		while(buf_pos < left) {
			cur_block = (start_offset + buf_pos) / fs->fs_block_size;
			err = vfs_bread(fs, cur_block, &bio);
			ANANAS_ERROR_RETURN(err);
			int piece_len = fs->fs_block_size - ((start_offset + buf_pos) % fs->fs_block_size);
			if (piece_len > left)
				piece_len = left;
			memcpy(fs_privdata->temp_buf + buf_pos, static_cast<void*>(static_cast<char*>(BIO_DATA(bio)) + ((start_offset + buf_pos) % fs->fs_block_size)), piece_len);
			buf_pos += piece_len;
		}

		cramfs_zstream.next_in = fs_privdata->temp_buf;
		cramfs_zstream.avail_in = left;

		cramfs_zstream.next_out = fs_privdata->decompress_buf;
		cramfs_zstream.avail_out = sizeof(fs_privdata->decompress_buf);

		int zerr = inflateReset(&cramfs_zstream);
		KASSERT(zerr == Z_OK, "inflateReset() error %d", zerr);
		zerr = inflate(&cramfs_zstream, Z_FINISH);
		KASSERT(zerr == Z_STREAM_END, "inflate() error %d", zerr);
		KASSERT(cramfs_zstream.total_out <= CRAMFS_PAGE_SIZE, "inflate() gave more data than a page");

		int copy_chunk = (cramfs_zstream.total_out > toread) ? toread :  cramfs_zstream.total_out;
		memcpy(buf, &fs_privdata->decompress_buf[file->f_offset % CRAMFS_PAGE_SIZE], copy_chunk);

		file->f_offset += copy_chunk;
		buf = static_cast<void*>(static_cast<char*>(buf) + copy_chunk);
		total += copy_chunk;
		toread -= copy_chunk;
	}

	*len = total;
	return ananas_success();
}

static errorcode_t
cramfs_readdir(struct VFS_FILE* file, void* dirents, size_t* len)
{
	struct VFS_INODE* inode = file->f_dentry->d_inode;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct CRAMFS_INODE_PRIVDATA* i_privdata = (struct CRAMFS_INODE_PRIVDATA*)inode->i_privdata;
	size_t written = 0, toread = *len;

	uint32_t cur_offset = i_privdata->offset + file->f_offset;
	uint32_t left = inode->i_sb.st_size - file->f_offset;
	CRAMFS_DEBUG_READDIR("cramfs_readdir(): privdata_offs=%u, cur_offset=%u, size=%u, left=%u\n",
	 i_privdata->offset, cur_offset, file->f_inode->i_sb.st_size, left);

	/*
	 * Note that cramfs does not have any alignment requirements on inodes; they
	 * may split across sectors as required. To ensure we do the right thing in
	 * such a case, keep a dummy inode which we use to assemble enough data to
	 * parse the entry.
	 */
	unsigned int partial_inode_len = 0;
	char partial_inode_data[sizeof(struct CRAMFS_INODE) + 64 /* max name len */];

	unsigned int cur_block = (unsigned int)-1;
	struct BIO* bio = NULL;
	while (toread > 0 && left > 0) {
		unsigned int new_block = cur_offset / fs->fs_block_size;
		if (new_block != cur_block) {
			if (bio != NULL) bio_free(bio);
			errorcode_t err = vfs_bread(fs, new_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = new_block;
		}

		auto cram_inode = reinterpret_cast<struct CRAMFS_INODE*>(static_cast<char*>(BIO_DATA(bio)) + cur_offset % fs->fs_block_size);
		if (partial_inode_len > 0) {
			/* Previous inode was partial; append whatever we can after it */
			memcpy(&partial_inode_data[partial_inode_len], BIO_DATA(bio), sizeof(partial_inode_data) - partial_inode_len);
			cram_inode = reinterpret_cast<struct CRAMFS_INODE*>(partial_inode_data);
			/*
			 * Now, 'loopback' the current offset - after processing the inode, the position will be updated
			 * and all will be well.
			 */
			cur_offset -= partial_inode_len;
			partial_inode_len = 0;
		} else {
			int partial_offset = cur_offset % fs->fs_block_size;
			if ((partial_offset + sizeof(struct CRAMFS_INODE) > fs->fs_block_size) ||
					(partial_offset + (CRAMFS_INODE_NAMELEN(CRAMFS_TO_LE32(cram_inode->in_namelen_offset)) * 4) > fs->fs_block_size)) {
				/* This inode is partial; we must store it and grab the next block */
				partial_inode_len = fs->fs_block_size - partial_offset;
				memcpy(partial_inode_data, cram_inode, partial_inode_len);
				cur_offset += partial_inode_len;
				continue;
			}
		}

		/* Names are contained directly after the inode data and padded to 4 bytes */
		int name_len = CRAMFS_INODE_NAMELEN(CRAMFS_TO_LE32(cram_inode->in_namelen_offset)) * 4;
		if (name_len == 0)
			panic("cramfs_readdir(): name_len=0! (inode %p)", cram_inode);

		/* Figure out the true name length, without terminating \0's */
		int real_name_len = 0;
		while (((char*)(cram_inode + 1))[real_name_len] != '\0' && real_name_len < name_len) {
			real_name_len++;
		}
		/* Update the offsets */
		int entry_len = sizeof(*cram_inode) + name_len;
		CRAMFS_DEBUG_READDIR("cramfs_readdir(): fsbs=%u, inode=%p, entry_len=%u, name_len=%u, namelenoffs=%x, cur_offset=%u, file_offset=%u, &namelen=%x\n",
		 entry_len, name_len, cram_inode->in_namelen_offset, cur_offset, (int)file->offset,
		 &cram_inode->in_namelen_offset);

		/* And hand it to the fill function */
		int filled = vfs_filldirent(&dirents, &toread, cur_offset, ((char*)(cram_inode + 1)), real_name_len);
		if (!filled) {
			/* out of space! */
			break;
		}
		written += filled;

		cur_offset += entry_len;
		file->f_offset += entry_len;
		KASSERT(left >= entry_len, "removing beyond directory inode length");
		left -= entry_len;
	}
	if (bio != NULL) bio_free(bio);

	CRAMFS_DEBUG_READDIR("cramfs_readdir(): done, returning %u bytes\n", written);
	*len = written;
	return ananas_success();
}

static struct VFS_INODE_OPS cramfs_file_ops = {
	.read = cramfs_read
};

static struct VFS_INODE_OPS cramfs_dir_ops = {
	.readdir = cramfs_readdir,
	.lookup = vfs_generic_lookup
};

static void
cramfs_convert_inode(uint32_t offset, struct CRAMFS_INODE* cinode, struct VFS_INODE* inode)
{
	struct CRAMFS_INODE_PRIVDATA* i_privdata = (struct CRAMFS_INODE_PRIVDATA*)inode->i_privdata;
	i_privdata->offset = CRAMFS_INODE_OFFSET(CRAMFS_TO_LE32(cinode->in_namelen_offset)) * 4;

	inode->i_sb.st_ino    = offset;
	inode->i_sb.st_mode   = CRAMFS_TO_LE16(cinode->in_mode);
	inode->i_sb.st_nlink  = 1;
	inode->i_sb.st_uid    = CRAMFS_TO_LE16(cinode->in_uid);
	inode->i_sb.st_gid    = CRAMFS_INODE_GID(CRAMFS_TO_LE32(cinode->in_size_gid));
	inode->i_sb.st_atime  = 0; /* XXX */
	inode->i_sb.st_mtime  = 0;
	inode->i_sb.st_ctime  = 0;
	inode->i_sb.st_blocks = 0;
	inode->i_sb.st_size   = CRAMFS_INODE_SIZE(CRAMFS_TO_LE32(cinode->in_size_gid));
	
	switch(inode->i_sb.st_mode & 0xf000) {
		case CRAMFS_S_IFREG:
			inode->i_iops = &cramfs_file_ops;
			break;
		case CRAMFS_S_IFDIR:
			inode->i_iops = &cramfs_dir_ops;
			break;
	}
}

static errorcode_t
cramfs_mount(struct VFS_MOUNTED_FS* fs, struct VFS_INODE** root_inode)
{
	fs->fs_block_size = 512;

	/* Fetch the first sector; we use it to validate the filesystem */
	struct BIO* bio;
	errorcode_t err = vfs_bread(fs, 0, &bio);
	ANANAS_ERROR_RETURN(err);
	auto sb = static_cast<struct CRAMFS_SUPERBLOCK*>(BIO_DATA(bio));
	if (CRAMFS_TO_LE32(sb->c_magic) != CRAMFS_MAGIC) {
		bio_free(bio);
		return ANANAS_ERROR(NO_DEVICE);
	}

	if ((CRAMFS_TO_LE32(sb->c_flags) & ~CRAMFS_FLAG_MASK) != 0) {
		bio_free(bio);
		kprintf("cramfs: unsupported flags, refusing to mount\n");
		return ANANAS_ERROR(NO_DEVICE);
	}

	fs->fs_privdata = new CRAMFS_PRIVDATA;

	/* Everything is ok; fill out the filesystem details */
	err = vfs_get_inode(fs, __builtin_offsetof(struct CRAMFS_SUPERBLOCK, c_rootinode), root_inode);
	if (ananas_is_failure(err)) {
		kfree(fs->fs_privdata);
		bio_free(bio);
		return ANANAS_ERROR(NO_DEVICE);
	}

	/* Initialize our deflater */
	cramfs_zstream.next_in = NULL;
	cramfs_zstream.avail_in = 0;
	inflateInit(&cramfs_zstream);

	return ananas_success();
}

static errorcode_t
cramfs_read_inode(struct VFS_INODE* inode, ino_t inum)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;

	/*
	 * Our fsop is just an offset in the filesystem, so all we have to do is
	 * grab the block in which the inode resides and convert the inode.
	 */
	uint32_t offset = inum;
	struct BIO* bio;
	errorcode_t err = vfs_bread(fs, offset / fs->fs_block_size, &bio);
	ANANAS_ERROR_RETURN(err);
	auto cram_inode = reinterpret_cast<struct CRAMFS_INODE*>(static_cast<char*>(BIO_DATA(bio)) + offset % fs->fs_block_size);
	cramfs_convert_inode(offset, cram_inode, inode);
	bio_free(bio);
	return ananas_success();
}

static errorcode_t
cramfs_prepare_inode(struct VFS_INODE* inode)
{
	auto i_privdata = new CRAMFS_INODE_PRIVDATA;
	memset(i_privdata, 0, sizeof(struct CRAMFS_INODE_PRIVDATA));
	inode->i_privdata = i_privdata;
	return ananas_success();
}

static void
cramfs_discard_inode(struct VFS_INODE* inode)
{
	kfree(inode->i_privdata);
}

static struct VFS_FILESYSTEM_OPS fsops_cramfs = {
	.mount = cramfs_mount,
	.prepare_inode = cramfs_prepare_inode,
	.discard_inode = cramfs_discard_inode,
	.read_inode = cramfs_read_inode
};

static struct VFS_FILESYSTEM fs_cramfs = {
	.fs_name = "cramfs",
	.fs_fsops = &fsops_cramfs
};

errorcode_t
cramfs_init()
{
	return vfs_register_filesystem(&fs_cramfs);
}

static errorcode_t
cramfs_exit()
{
	return vfs_unregister_filesystem(&fs_cramfs);
}

INIT_FUNCTION(cramfs_init, SUBSYSTEM_VFS, ORDER_MIDDLE);
EXIT_FUNCTION(cramfs_exit);

/* vim:set ts=2 sw=2: */
