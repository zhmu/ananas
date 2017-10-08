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
#include <ananas/error.h>
#include <iso9660.h>
#include "kernel/bio.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/generic.h"
#include "kernel/vfs/mount.h"

TRACE_SETUP;

#define ISO9660_GET_WORD(x) (((uint16_t)(x)[0]) | ((uint16_t)(x)[1] << 8))
#define ISO9660_GET_DWORD(x) (((uint32_t)(x)[0]) | ((uint32_t)(x)[1] << 8) | ((uint32_t)(x)[2] << 16) | ((uint32_t)(x)[3] << 24))

struct ISO9660_INODE_PRIVDATA {
	uint32_t lba;
};

namespace {
extern struct VFS_INODE_OPS iso9660_dir_ops;
extern struct VFS_INODE_OPS iso9660_file_ops;
} // unnamed namespace

#if 0
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
#endif

static errorcode_t
iso9660_mount(struct VFS_MOUNTED_FS* fs, struct VFS_INODE** root_inode)
{
	struct ISO9660_DIRECTORY_ENTRY* rootentry = nullptr;

	/* Obtain the primary volume descriptor; it contains vital information */
	fs->fs_block_size = 2048;
	struct BIO* bio;
	errorcode_t err = vfs_bread(fs, 4, &bio);
	ANANAS_ERROR_RETURN(err);

	/* Verify the primary volume descriptor */
	struct ISO9660_PRIMARY_VOLUME_DESCR* pvd = (struct ISO9660_PRIMARY_VOLUME_DESCR*)BIO_DATA(bio);
	err = ANANAS_ERROR(NO_DEVICE);
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
	rootentry = (struct ISO9660_DIRECTORY_ENTRY*)pvd->pv_root_direntry;
	if (rootentry->de_length != 34 || rootentry->de_filename_len != 1 || rootentry->de_filename[0] != '\0' ||
			(rootentry->de_flags & DE_FLAG_DIRECTORY) == 0) {
		kprintf("iso9660: root entry corrupted\n");
		goto fail;
	}
	fs->fs_block_size = ISO9660_GET_WORD(pvd->pv_blocksize);

	/* Read the root inode */
	err = vfs_get_inode(fs, 0, root_inode);
	if (ananas_is_failure(err))
		goto fail;

	{
		auto privdata = static_cast<struct ISO9660_INODE_PRIVDATA*>((*root_inode)->i_privdata);
		(*root_inode)->i_sb.st_size = ISO9660_GET_DWORD(rootentry->de_data_length);
		(*root_inode)->i_iops = &iso9660_dir_ops;
		privdata->lba = ISO9660_GET_DWORD(rootentry->de_extent_lba);
	}

	err = ananas_success();

fail:
	bio_free(bio);
	return err;
}

static errorcode_t
iso9660_read_inode(struct VFS_INODE* inode, ino_t inum)
{
	uint32_t block = inum >> 16;
	uint16_t offset = inum & 0xffff;
	KASSERT(offset < inode->i_fs->fs_block_size + sizeof(struct ISO9660_DIRECTORY_ENTRY), "offset does not reside in block");

	/* Grab the block containing the inode */
	struct BIO* bio;
	errorcode_t err = vfs_bread(inode->i_fs, block, &bio);
	ANANAS_ERROR_RETURN(err);

	/* Convert the inode */
	auto iso9660_de = reinterpret_cast<struct ISO9660_DIRECTORY_ENTRY*>(static_cast<char*>(BIO_DATA(bio)) + offset);
	struct ISO9660_INODE_PRIVDATA* privdata = (struct ISO9660_INODE_PRIVDATA*)inode->i_privdata;
	inode->i_sb.st_ino = block << 16 | offset;
	inode->i_sb.st_mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; /* r-xr-xr-x */
	inode->i_sb.st_nlink = 1;
	inode->i_sb.st_uid = 0;
	inode->i_sb.st_gid = 0;
	inode->i_sb.st_size = ISO9660_GET_DWORD(iso9660_de->de_data_length);
	inode->i_sb.st_atime = 0; /* XXX */
	inode->i_sb.st_mtime = 0; /* XXX */
	inode->i_sb.st_ctime = 0; /* XXX */
	inode->i_sb.st_blocks = inode->i_sb.st_size / inode->i_fs->fs_block_size;

	privdata->lba = ISO9660_GET_DWORD(iso9660_de->de_extent_lba);
	if (iso9660_de->de_flags & DE_FLAG_DIRECTORY) {
		inode->i_iops = &iso9660_dir_ops;
		inode->i_sb.st_mode |= S_IFDIR;
	} else {
		inode->i_iops = &iso9660_file_ops;
		inode->i_sb.st_mode |= S_IFREG;
	}
	return ananas_success();
}

static errorcode_t
iso9660_prepare_inode(struct VFS_INODE* inode)
{
	auto privdata = new ISO9660_INODE_PRIVDATA;
	memset(privdata, 0, sizeof(struct ISO9660_INODE_PRIVDATA));
	inode->i_privdata = privdata;
	return ananas_success();
}

static void
iso9660_discard_inode(struct VFS_INODE* inode)
{
	kfree(inode->i_privdata);
}

static errorcode_t
iso9660_readdir(struct VFS_FILE* file, void* dirents, size_t* len)
{
	struct VFS_INODE* inode = file->f_dentry->d_inode;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	auto privdata = static_cast<struct ISO9660_INODE_PRIVDATA*>(inode->i_privdata);
	blocknr_t block = privdata->lba + file->f_offset / fs->fs_block_size;
	uint32_t offset = file->f_offset % fs->fs_block_size;
	size_t written = 0, left = *len;

	struct BIO* bio = NULL;
	blocknr_t curblock = 0;
	while(left > 0) {
		if (block > privdata->lba + inode->i_sb.st_size / fs->fs_block_size) {
			/*
			 * We've run out of blocks. Need to stop here.
			 */
			break;
		}
		if(curblock != block) {
			if (bio != NULL) bio_free(bio);
			errorcode_t err = vfs_bread(fs, block, &bio);
			ANANAS_ERROR_RETURN(err);
			curblock = block;
		}

		/*
		 * Take the directory entry. Note that entries are not allowed to cross
		 * multiple sectors.
		 */
		auto iso9660de = reinterpret_cast<struct ISO9660_DIRECTORY_ENTRY*>(static_cast<char*>(BIO_DATA(bio)) + offset);
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

			ino_t inum = (ino_t)curblock << 16 | offset;
			int filled = vfs_filldirent(&dirents, &left, inum, (const char*)iso9660de->de_filename, iso9660de->de_filename_len);
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
		file->f_offset += iso9660de->de_length;
		offset += iso9660de->de_length;
		if (offset >= fs->fs_block_size) {
			offset -= fs->fs_block_size;
			block++;
		}
	}
	if (bio != NULL) bio_free(bio);
	*len = written;
	return ananas_success();
}

static errorcode_t
iso9660_read(struct VFS_FILE* file, void* buf, size_t* len)
{
	struct VFS_INODE* inode = file->f_dentry->d_inode;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	auto privdata = static_cast<struct ISO9660_INODE_PRIVDATA*>(inode->i_privdata);
	blocknr_t blocknum = (blocknr_t)file->f_offset / fs->fs_block_size;
	uint32_t offset = file->f_offset % fs->fs_block_size;
	size_t numread = 0, left = *len;

	/* Normalize len so that it cannot expand beyond the file size */
	if (file->f_offset + left > inode->i_sb.st_size)
		left = inode->i_sb.st_size - file->f_offset;

	while(left > 0) {
		/* Fetch the block */
		struct BIO* bio;
		errorcode_t err = vfs_bread(fs, privdata->lba + blocknum, &bio);
		ANANAS_ERROR_RETURN(err);

		/* Copy what we can so far */
		size_t chunklen = (fs->fs_block_size < left ? fs->fs_block_size : left);
		if (chunklen + offset > fs->fs_block_size)
			chunklen = fs->fs_block_size - offset;
		memcpy(buf, reinterpret_cast<void*>(static_cast<char*>(BIO_DATA(bio)) + offset), chunklen);
		buf = reinterpret_cast<void*>(static_cast<char*>(buf) + chunklen);
		numread += chunklen; left -= chunklen;
		bio_free(bio);

		/* Update pointers */
		offset = (offset + chunklen) % fs->fs_block_size;
		file->f_offset += chunklen;
		blocknum++;
	}

	*len = numread;
	return ananas_success();
}

namespace {
struct VFS_INODE_OPS iso9660_dir_ops = {
	.readdir = iso9660_readdir,
	.lookup = vfs_generic_lookup 
};

struct VFS_INODE_OPS iso9660_file_ops = {
	.read = iso9660_read,
};

struct VFS_FILESYSTEM_OPS fsops_iso9660 = {
	.mount = iso9660_mount,
	.prepare_inode = iso9660_prepare_inode,
	.discard_inode = iso9660_discard_inode,
	.read_inode = iso9660_read_inode
};

struct VFS_FILESYSTEM fs_iso9660 = {
	.fs_name = "iso9660",
	.fs_fsops = &fsops_iso9660
};

} // unnamed namespace

errorcode_t
iso9660_init()
{
	return vfs_register_filesystem(&fs_iso9660);
}

static errorcode_t
iso9660_exit()
{
	return vfs_unregister_filesystem(&fs_iso9660);
}

INIT_FUNCTION(iso9660_init, SUBSYSTEM_VFS, ORDER_MIDDLE);
EXIT_FUNCTION(iso9660_exit);

/* vim:set ts=2 sw=2: */
