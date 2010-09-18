/*
 * Ananas ISO9660 filesystem driver.
 *
 * Note that, like Linux, we ignore the path table completely as it is limited
 * to 65536 entries. It will be enough to just scan the root directory entry
 * and walk through the records sequentially. This is slower, but it works on
 * any filesystem (and if we cache enough, speed loss shouldn't be that
 * noticable)
 */
#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/vfs.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <iso9660.h>

/* XXX These macros need to be updated for non-LE architectures */
#define ISO9660_GET_WORD(x) (*(uint16_t*)(x))
#define ISO9660_GET_DWORD(x) (*(uint32_t*)(x))

struct ISO9660_INODE_PRIVDATA {
	uint32_t lba;
};
	
static struct VFS_INODE_OPS iso9660_dir_ops;
static struct VFS_INODE_OPS iso9660_file_ops;

static void
iso9660_dump_dirent(struct ISO9660_DIRECTORY_ENTRY* e)
{
	kprintf("direntry %p\n", e);
	kprintf("length         = %x\n", e->de_length);
	kprintf("ea_length      = %x\n", e->de_ea_length);
	kprintf("extent_lba     = %x\n", ISO9660_GET_DWORD(e->de_extent_lba));
	kprintf("data_length    = %x\n", ISO9660_GET_DWORD(e->de_data_length));
	kprintf("flags          = %x\n", e->de_flags);
	kprintf("unit size      = %x\n", e->de_unitsize);
	kprintf("interleave gap = %x\n", e->de_interleave_gap);
	kprintf("filename       = %u'", e->de_filename_len);
	for (unsigned int i = 0; i < e->de_filename_len; i++)
		kprintf("%c", e->de_filename[i]);
	kprintf("'\n");
}

static int
iso9660_mount(struct VFS_MOUNTED_FS* fs)
{
	int result = 0;

	struct BIO* bio = vfs_bread(fs, 16, 2048);
	/* XXX handle errors */

	struct ISO9660_PRIMARY_VOLUME_DESCR* pvd = (struct ISO9660_PRIMARY_VOLUME_DESCR*)BIO_DATA(bio);
	if (pvd->pv_typecode != 1 || memcmp(pvd->pv_stdentry, "CD001", 5) || pvd->pv_version != 1 || pvd->pv_structure_version != 1) {
		/* Not an ISO9660 filesystem */
		goto fail;
	}
	if (ISO9660_GET_WORD(pvd->pv_setsize) != 1) {
		kprintf("iso9660: cannot handle sets larger than 1 volume (this one is %u)\n", ISO9660_GET_WORD(pvd->pv_setsize));
		goto fail;
	}
	if (ISO9660_GET_WORD(pvd->pv_blocksize) != 2048) {
		kprintf("iso9660: cannot handle blocksize != 2048 (this one is %u)\n", ISO9660_GET_WORD(pvd->pv_blocksize));
		goto fail;
	}

	/*
	 * ISO9660 stores the root directory entry in the primary volume descriptor,
	 * so we need to copy it from there.
	 */
	struct ISO9660_DIRECTORY_ENTRY* rootentry = (struct ISO9660_DIRECTORY_ENTRY*)pvd->pv_root_direntry;
	if (rootentry->de_length != 34 || rootentry->de_filename_len != 1 || rootentry->de_filename[0] != '\0' ||
	    (rootentry->de_flags & DE_FLAG_DIRECTORY) == 0) {
		kprintf("iso9660: root entry corrupted\n");
		goto fail;
	}
  fs->root_inode = vfs_alloc_inode(fs);
	fs->block_size = ISO9660_GET_WORD(pvd->pv_blocksize);
	fs->fsop_size = sizeof(uint64_t);

	struct ISO9660_INODE_PRIVDATA* privdata = (struct ISO9660_INODE_PRIVDATA*)fs->root_inode->privdata;
	fs->root_inode->sb.st_size = ISO9660_GET_DWORD(rootentry->de_data_length);
	fs->root_inode->iops = &iso9660_dir_ops;
	privdata->lba = ISO9660_GET_DWORD(rootentry->de_extent_lba);

	result = 1;

fail:
	bio_free(bio);
	return result;
}

static int
iso9660_read_inode(struct VFS_INODE* inode, void* fsop)
{
	uint64_t iso9660_fsop = *(uint64_t*)fsop;
	uint32_t block = iso9660_fsop >> 16;
	uint16_t offset = iso9660_fsop & 0xffff;
	KASSERT(offset < inode->fs->block_size + sizeof(struct ISO9660_DIRECTORY_ENTRY), "offset does not reside in block");

	struct BIO* bio = vfs_bread(inode->fs, block, inode->fs->block_size);
	/* XXX error handling */

	/* Convert the inode */
	struct ISO9660_DIRECTORY_ENTRY* iso9660_de = (struct ISO9660_DIRECTORY_ENTRY*)(BIO_DATA(bio) + offset);
	struct ISO9660_INODE_PRIVDATA* privdata = (struct ISO9660_INODE_PRIVDATA*)inode->privdata;
	inode->sb.st_ino = block << 16 | offset;
	inode->sb.st_mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; /* r-xr-xr-x */
	inode->sb.st_nlink = 1;
	inode->sb.st_uid = 0;
	inode->sb.st_gid = 0;
	inode->sb.st_size = ISO9660_GET_DWORD(iso9660_de->de_data_length);
	inode->sb.st_atime = 0; /* XXX */
	inode->sb.st_mtime = 0; /* XXX */
	inode->sb.st_ctime = 0; /* XXX */
	inode->sb.st_blocks = inode->sb.st_size / inode->fs->block_size;

	privdata->lba = ISO9660_GET_DWORD(iso9660_de->de_extent_lba);
	if (iso9660_de->de_flags & DE_FLAG_DIRECTORY) {
		inode->iops = &iso9660_dir_ops;
		inode->sb.st_mode |= S_IFDIR;
	} else {
		inode->iops = &iso9660_file_ops;
		inode->sb.st_mode |= S_IFREG;
	}
	return 1;
}

static struct VFS_INODE*
iso9660_alloc_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode = vfs_make_inode(fs);
	if (inode == NULL)
		return NULL;

	struct ISO9660_INODE_PRIVDATA* privdata = kmalloc(sizeof(struct ISO9660_INODE_PRIVDATA));
	memset(privdata, 0, sizeof(struct ISO9660_INODE_PRIVDATA));
	inode->privdata = privdata;
	return inode;
}

static void
iso9660_free_inode(struct VFS_INODE* inode)
{
	kfree(inode->privdata);
	vfs_destroy_inode(inode);
}

static size_t
iso9660_readdir(struct VFS_FILE* file, void* dirents, size_t entsize)
{
	struct VFS_MOUNTED_FS* fs = file->inode->fs;
	struct ISO9660_INODE_PRIVDATA* privdata = (struct ISO9660_INODE_PRIVDATA*)file->inode->privdata;
	block_t block = privdata->lba + file->offset / fs->block_size;
	uint32_t offset = file->offset % fs->block_size;
	size_t written = 0;

	struct BIO* bio = NULL;
	block_t curblock = 0;
	while(entsize > 0) {
		if (block > privdata->lba + file->inode->sb.st_size / fs->block_size) {
			/*
			 * We've run out of blocks. Need to stop here.
			 */
			break;
		}
		if(curblock != block) {
			if (bio != NULL) bio_free(bio);
			bio = vfs_bread(fs, block, fs->block_size);
			/* XXX handle error */
			curblock = block;
		}

		/*
		 * Take the directory entry. Note that entries are not allowed to cross
		 * multiple sectors.
		 */
		struct ISO9660_DIRECTORY_ENTRY* iso9660de = (struct ISO9660_DIRECTORY_ENTRY*)(void*)(BIO_DATA(bio) + offset);
		if (iso9660de->de_length == 0)
			break;

		/* Only process entries that must not be hidden */
		if ((iso9660de->de_flags & DE_FLAG_HIDDEN) == 0) {
			/*
			 * ISO9660 stores ';file ID' (;1) after each entry; we just chop it off. It
			 * also likes to use uppercase, which we trim off here as well.
			 */
			if (iso9660de->de_filename_len > 2) {
				if (iso9660de->de_filename[iso9660de->de_filename_len - 2] == ';' &&
						iso9660de->de_filename[iso9660de->de_filename_len - 1] == '1')
					iso9660de->de_filename_len -= 2;
				for (int i = 0; i <  iso9660de->de_filename_len; i++) {
					if (iso9660de->de_filename[i] >= 'A' && iso9660de->de_filename[i] <= 'Z')
						iso9660de->de_filename[i] += 'a' - 'A';
				}

				/* If the final charachter is only a '.', kill it */
				if (iso9660de->de_filename_len > 0 && iso9660de->de_filename[iso9660de->de_filename_len - 1] == '.')
					iso9660de->de_filename_len--;
			}

			uint64_t fsop = (uint64_t)curblock << 16 | offset;
			int filled = vfs_filldirent(&dirents, &entsize, (const void*)&fsop, file->inode->fs->fsop_size, iso9660de->de_filename, iso9660de->de_filename_len);
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
		file->offset += iso9660de->de_length;
		offset += iso9660de->de_length;
		if (offset >= fs->block_size) {
			offset -= fs->block_size;
			block++;
		}
	}
	if (bio != NULL) bio_free(bio);
	return written;
}

struct VFS_INODE*
iso9660_lookup(struct VFS_INODE* dirinode, const char* dentry)
{
	struct VFS_FILE dir;
	char tmp[1024]; /* XXX */
	char* tmp_ptr = tmp;

	/*
	 * XXX This is a very naive implementation which does not use the
	 * possible directory index.
	 */
	dir.offset = 0;
	dir.inode = dirinode;
	while (1) {
		size_t left = dirinode->iops->readdir(&dir, tmp, sizeof(tmp));
		if (left <= 0)
			break;

		while (left > 0) {
			struct VFS_DIRENT* de = (struct VFS_DIRENT*)tmp_ptr;
			left -= DE_LENGTH(de); tmp_ptr += DE_LENGTH(de);
			
			if (strcmp(de->de_fsop + de->de_fsop_length, dentry) != 0)
				continue;

			/* Found it! */
			struct VFS_INODE* inode = vfs_get_inode(dirinode->fs, de->de_fsop);
			if (inode == NULL)
				return NULL;
			return inode;
		}
	}

	/* Not found */
	return NULL;
}

static size_t
iso9660_read(struct VFS_FILE* file, void* buf, size_t len)
{
	struct VFS_MOUNTED_FS* fs = file->inode->fs;
	struct ISO9660_INODE_PRIVDATA* privdata = (struct ISO9660_INODE_PRIVDATA*)file->inode->privdata;
	block_t blocknum = (block_t)file->offset / fs->block_size;
	uint32_t offset = file->offset % fs->block_size;
	size_t numread = 0;

	/* Normalize len so that it cannot expand beyond the file size */
	if (file->offset + len > file->inode->sb.st_size)
		len = file->inode->sb.st_size - file->offset;

	while(len > 0) {
		/*
		 * Fetch the block and copy what we have so far.
		 */
		struct BIO* bio = vfs_bread(fs, privdata->lba + blocknum, fs->block_size);
		size_t chunklen = (fs->block_size < len ? fs->block_size : len);
		if (chunklen + offset > fs->block_size)
			chunklen = fs->block_size - offset;
		memcpy(buf, (void*)(BIO_DATA(bio) + offset), chunklen);
		buf += chunklen; numread += chunklen; len -= chunklen;
		bio_free(bio);

		/* Update pointers */
		offset = (offset + chunklen) % fs->block_size;
		file->offset += chunklen;
		blocknum++;
	}

	return numread;
}

static struct VFS_INODE_OPS iso9660_dir_ops = {
	.readdir = iso9660_readdir,
	.lookup = iso9660_lookup
};

static struct VFS_INODE_OPS iso9660_file_ops = {
	.read = iso9660_read,
};

struct VFS_FILESYSTEM_OPS fsops_iso9660 = {
	.mount = iso9660_mount,
	.alloc_inode = iso9660_alloc_inode,
	.free_inode = iso9660_free_inode,
	.read_inode = iso9660_read_inode
};

/* vim:set ts=2 sw=2: */
