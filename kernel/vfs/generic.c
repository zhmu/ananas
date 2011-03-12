#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>

TRACE_SETUP;

#undef DEBUG_VFS_LOOKUP

errorcode_t
vfs_generic_lookup(struct VFS_INODE* dirinode, struct VFS_INODE** destinode, const char* dentry)
{
	struct VFS_FILE dir;
	char buf[1024]; /* XXX */

#ifdef DEBUG_VFS_LOOKUP
	kprintf("vfs_generic_lookup: target='%s'\n", dentry);
#endif

	/*
	 * XXX This is a very naive implementation which does not use the
	 * possible directory index.
	 */
	KASSERT(S_ISDIR(dirinode->i_sb.st_mode), "supplied inode is not a directory");
	memset(&dir, 0, sizeof(dir));
	dir.f_inode = dirinode;
	while (1) {
		size_t buf_len = sizeof(buf);
		int result = vfs_read(&dir, buf, &buf_len);
		if (result != ANANAS_ERROR_NONE)
			return result;
		if (buf_len == 0)
			return ANANAS_ERROR(NO_FILE);

		char* cur_ptr = buf;
		while (buf_len > 0) {
			struct VFS_DIRENT* de = (struct VFS_DIRENT*)cur_ptr;
			buf_len -= DE_LENGTH(de); cur_ptr += DE_LENGTH(de);

#ifdef DEBUG_VFS_LOOKUP
			kprintf("vfs_generic_lookup('%s'): comparing with '%s'\n", dentry, de->de_fsop + de->de_fsop_length);
#endif
	
			if (strcmp(de->de_fsop + de->de_fsop_length, dentry) != 0)
				continue;

			/* Found it! */
			return vfs_get_inode(dirinode->i_fs, de->de_fsop, destinode);
		}
	}
}

errorcode_t
vfs_generic_read(struct VFS_FILE* file, void* buf, size_t* len)
{
	struct VFS_INODE* inode = file->f_inode;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	size_t read = 0;
	size_t left = *len;
	struct BIO* bio = NULL;

	KASSERT(inode->i_iops->block_map != NULL, "called without block_map implementation");

	block_t cur_block = 0;
	while(left > 0) {
		/* Figure out which block to use next */
		block_t want_block;
		errorcode_t err = inode->i_iops->block_map(inode, (file->f_offset / (block_t)fs->fs_block_size), &want_block, 0);
		ANANAS_ERROR_RETURN(err);

		/* Grab the next block if necessary */
		if (cur_block != want_block || bio == NULL) {
			if (bio != NULL) bio_free(bio);
			err = vfs_bread(fs, want_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		/* Copy as much from the current entry as we can */
		off_t cur_offset = file->f_offset % (block_t)fs->fs_block_size;
		int chunk_len = fs->fs_block_size - cur_offset;
		if (chunk_len > left)
			chunk_len = left;
		KASSERT(chunk_len > 0, "attempt to handle empty chunk");
		memcpy(buf, (void*)(BIO_DATA(bio) + cur_offset), chunk_len);

		read += chunk_len;
		buf += chunk_len;
		left -= chunk_len;
		file->f_offset += chunk_len;
	}
	if (bio != NULL) bio_free(bio);
	*len = read;
	return ANANAS_ERROR_OK;
}

errorcode_t
vfs_generic_write(struct VFS_FILE* file, void* buf, size_t* len)
{
	struct VFS_INODE* inode = file->f_inode;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	size_t written = 0;
	size_t left = *len;
	struct BIO* bio = NULL;

	KASSERT(inode->i_iops->block_map != NULL, "called without block_map implementation");

	block_t cur_block = 0;
	while(left > 0) {
		int create = 0;
		block_t logical_block = file->f_offset / (block_t)fs->fs_block_size;
		if (logical_block >= inode->i_sb.st_blocks /* XXX is this correct with sparse files? */)
			create++;

		/* Figure out which block to use next */
		block_t want_block;
		errorcode_t err = inode->i_iops->block_map(inode, logical_block, &want_block, create);
		ANANAS_ERROR_RETURN(err);

		/* Calculate how much we have to put in the block */
		off_t cur_offset = file->f_offset % (block_t)fs->fs_block_size;
		int chunk_len = fs->fs_block_size - cur_offset;
		if (chunk_len > left)
			chunk_len = left;

		/* Grab the next block if necessary */
		if (cur_block != want_block || bio == NULL) {
			if (bio != NULL) bio_free(bio);
			/* Only read the block if it's a new one or we're not replacing everything */
			err = vfs_bget(fs, want_block, &bio, (create || chunk_len == fs->fs_block_size) ? BIO_READ_NODATA : 0);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		/* Copy as much tp the block as we can */
		KASSERT(chunk_len > 0, "attempt to handle empty chunk");
		memcpy((void*)(BIO_DATA(bio) + cur_offset), buf, chunk_len);
		bio_set_dirty(bio);

		if (create)
			inode->i_sb.st_size += chunk_len;

		written += chunk_len;
		buf += chunk_len;
		left -= chunk_len;
		file->f_offset += chunk_len;
	}
	if (bio != NULL) bio_free(bio);
	*len = written;
	return ANANAS_ERROR_OK;
}

/* vim:set ts=2 sw=2: */
