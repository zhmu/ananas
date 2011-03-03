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
#include <ananas/bio.h>
#include <ananas/error.h>
#include <ananas/vfs.h>
#include <ananas/vfs/generic.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <ananas/zlib.h>
#include <cramfs.h>

TRACE_SETUP;

#define CRAMFS_DEBUG_READDIR(x...)

#define CRAMFS_PAGE_SIZE 4096

#define CRAMFS_TO_LE16(x) (x)
#define CRAMFS_TO_LE32(x) (x)

static z_stream cramfs_zstream;

struct CRAMFS_INODE_PRIVDATA {
	uint32_t offset;
};

static unsigned char temp_buf[CRAMFS_PAGE_SIZE * 2]; /* XXX */
static unsigned char decompress_buf[CRAMFS_PAGE_SIZE + 4]; /* XXX */

static struct VFS_INODE* cramfs_alloc_inode(struct VFS_MOUNTED_FS* fs);

static errorcode_t
cramfs_read(struct VFS_FILE* file, void* buf, size_t* len)
{
	struct VFS_MOUNTED_FS* fs = file->f_inode->i_fs;
	struct CRAMFS_INODE_PRIVDATA* privdata = (struct CRAMFS_INODE_PRIVDATA*)file->f_inode->i_privdata;

	size_t total = 0, toread = *len;
	while (toread > 0) {
		uint32_t page_index = file->f_offset / CRAMFS_PAGE_SIZE;
		uint32_t next_offset = 0;
		int cur_block = -1;
		struct BIO* bio = NULL;

		/* Calculate the compressed data offset of this page */
		cur_block = (privdata->offset + page_index * sizeof(uint32_t)) / fs->fs_block_size;
		bio = vfs_bread(fs, cur_block, fs->fs_block_size);
		/* XXX errors */
		next_offset = *(uint32_t*)(BIO_DATA(bio) + (privdata->offset + page_index * sizeof(uint32_t)) % fs->fs_block_size);

		uint32_t start_offset = 0;
		if (page_index > 0) {
			/* Now, fetch the offset of the previous page; this gives us the length of the compressed chunk */
			int prev_block = (privdata->offset + (page_index - 1) * sizeof(uint32_t)) / fs->fs_block_size;
			if (cur_block != prev_block) {
				bio = vfs_bread(fs, prev_block, fs->fs_block_size);
				/* XXX errors */
			}
			start_offset = *(uint32_t*)(BIO_DATA(bio) + (privdata->offset + (page_index - 1) * sizeof(uint32_t)) % fs->fs_block_size);
		} else {
			/* In case of the first page, we have to set the offset ourselves as there is no index we can use */
			start_offset  = privdata->offset;
			start_offset += (((file->f_inode->i_sb.st_size - 1) / CRAMFS_PAGE_SIZE) + 1) * sizeof(uint32_t);
		}

		uint32_t left = next_offset - start_offset;
		KASSERT(left < sizeof(temp_buf), "chunk too large");

		uint32_t buf_pos = 0;
		while(buf_pos < left) {
			cur_block = (start_offset + buf_pos) / fs->fs_block_size;
			bio = vfs_bread(fs, cur_block, fs->fs_block_size);
			/* XXX errors */
			int piece_len = fs->fs_block_size - ((start_offset + buf_pos) % fs->fs_block_size);
			if (piece_len > left)
				piece_len = left;
			memcpy(temp_buf + buf_pos, (void*)(BIO_DATA(bio) + ((start_offset + buf_pos) % fs->fs_block_size)), piece_len);
			buf_pos += piece_len;
		}

		cramfs_zstream.next_in = temp_buf;
		cramfs_zstream.avail_in = left;

		cramfs_zstream.next_out = decompress_buf;
		cramfs_zstream.avail_out = sizeof(decompress_buf);

		int err = inflateReset(&cramfs_zstream);
		KASSERT(err == Z_OK, "inflateReset() error %d", err);
		err = inflate(&cramfs_zstream, Z_FINISH);
		KASSERT(err == Z_STREAM_END, "inflate() error %d", err);
		KASSERT(cramfs_zstream.total_out <= CRAMFS_PAGE_SIZE, "inflate() gave more data than a page");

		int copy_chunk = (cramfs_zstream.total_out > toread) ? toread :  cramfs_zstream.total_out;
		memcpy(buf, &decompress_buf[file->f_offset % CRAMFS_PAGE_SIZE], copy_chunk);

		file->f_offset += copy_chunk;
		buf += copy_chunk;
		total += copy_chunk;
		toread -= copy_chunk;
	}

	*len = total;
	return ANANAS_ERROR_OK;
}

static errorcode_t
cramfs_readdir(struct VFS_FILE* file, void* dirents, size_t* len)
{
	struct VFS_MOUNTED_FS* fs = file->f_inode->i_fs;
	struct CRAMFS_INODE_PRIVDATA* privdata = (struct CRAMFS_INODE_PRIVDATA*)file->f_inode->i_privdata;
	size_t written = 0, toread = *len;

	uint32_t cur_offset = privdata->offset + file->f_offset;
	uint32_t left = file->f_inode->i_sb.st_size - file->f_offset;
	CRAMFS_DEBUG_READDIR("cramfs_readdir(): privdata_offs=%u, cur_offset=%u, size=%u, left=%u\n",
	 privdata->offset, cur_offset, file->f_inode->i_sb.st_size, left);

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
			bio = vfs_bread(fs, new_block, fs->fs_block_size);
			/* XXX errors */
			cur_block = new_block;
		}

		struct CRAMFS_INODE* cram_inode = (void*)((addr_t)BIO_DATA(bio) + cur_offset % fs->fs_block_size);
		if (partial_inode_len > 0) {
			/* Previous inode was partial; append whatever we can after it */
			memcpy(&partial_inode_data[partial_inode_len], BIO_DATA(bio), sizeof(partial_inode_data) - partial_inode_len);
			cram_inode = (void*)partial_inode_data;
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
		uint32_t fsop = cur_offset;
		int filled = vfs_filldirent(&dirents, &toread, (const void*)&fsop, file->f_inode->i_fs->fs_fsop_size, ((char*)(cram_inode + 1)), real_name_len);
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
	return ANANAS_ERROR_OK;
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
	struct CRAMFS_INODE_PRIVDATA* privdata = (struct CRAMFS_INODE_PRIVDATA*)inode->i_privdata;
	privdata->offset = CRAMFS_INODE_OFFSET(CRAMFS_TO_LE32(cinode->in_namelen_offset)) * 4;

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
cramfs_mount(struct VFS_MOUNTED_FS* fs)
{
	struct BIO* bio = vfs_bread(fs, 0, 512);
	/* XXX errors (from a ramdisk?) */
	struct CRAMFS_SUPERBLOCK* sb = BIO_DATA(bio);
	if (CRAMFS_TO_LE32(sb->c_magic) != CRAMFS_MAGIC) {
		bio_free(bio);
		return ANANAS_ERROR(NO_DEVICE);
	}

	if ((CRAMFS_TO_LE32(sb->c_flags) & ~CRAMFS_FLAG_MASK) != 0) {
		kprintf("cramfs: unsupported flags, refusing to mount\n");
		return ANANAS_ERROR(NO_DEVICE);
	}

	/* Initialize the inode cache right before reading the root directory inode */
	fs->fs_fsop_size = sizeof(uint32_t);
	icache_init(fs);

	/* Everything is ok; fill out the filesystem details */
	fs->fs_block_size = 512;
	fs->fs_root_inode = cramfs_alloc_inode(fs);
	cramfs_convert_inode((addr_t)&sb->c_rootinode - (addr_t)sb, &sb->c_rootinode, fs->fs_root_inode);

	/* Initialize our deflater */
	cramfs_zstream.next_in = NULL;
	cramfs_zstream.avail_in = 0;
	inflateInit(&cramfs_zstream);

	return ANANAS_ERROR_OK;
}

static errorcode_t
cramfs_read_inode(struct VFS_INODE* inode, void* fsop)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;

	uint32_t offset = *(uint32_t*)fsop;

	struct BIO* bio = vfs_bread(fs, offset / fs->fs_block_size, fs->fs_block_size);
	/* XXX errors */
	struct CRAMFS_INODE* cram_inode = (void*)((addr_t)BIO_DATA(bio) + offset % fs->fs_block_size);

	cramfs_convert_inode(offset, cram_inode, inode);

	bio_free(bio);

	return ANANAS_ERROR_OK;
}

static struct VFS_INODE*
cramfs_alloc_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode = vfs_make_inode(fs);
	if (inode == NULL)
		return NULL;
	struct CRAMFS_INODE_PRIVDATA* privdata = kmalloc(sizeof(struct CRAMFS_INODE_PRIVDATA));
	memset(privdata, 0, sizeof(struct CRAMFS_INODE_PRIVDATA));
	inode->i_privdata = privdata;
	return inode;
}

static void
cramfs_destroy_inode(struct VFS_INODE* inode)
{
	kfree(inode->i_privdata);
	vfs_destroy_inode(inode);
}

struct VFS_FILESYSTEM_OPS fsops_cramfs = {
	.mount = cramfs_mount,
	.alloc_inode = cramfs_alloc_inode,
	.destroy_inode = cramfs_destroy_inode,
	.read_inode = cramfs_read_inode
};

/* vim:set ts=2 sw=2: */
