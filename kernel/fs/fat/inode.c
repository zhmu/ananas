#include <ananas/types.h>
#include <ananas/vfs/types.h>
#include <ananas/vfs/core.h>
#include <ananas/vfs/generic.h>
#include <ananas/bio.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/lock.h>
#include <fat.h>
#include "block.h"
#include "dir.h"
#include "fatfs.h"
#include "inode.h"

struct VFS_INODE*
fat_alloc_inode(struct VFS_MOUNTED_FS* fs, const void* fsop)
{
	struct VFS_INODE* inode = vfs_make_inode(fs, fsop);
	if (inode == NULL)
		return NULL;
	inode->i_privdata = kmalloc(sizeof(struct FAT_INODE_PRIVDATA));
	memset(inode->i_privdata, 0, sizeof(struct FAT_INODE_PRIVDATA));
	return inode;
}

void
fat_destroy_inode(struct VFS_INODE* inode)
{
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;

	/*
	 * If the file has backing storage, we need to throw away all inode's cluster
	 * items in the cache.
	 */
	if (privdata->first_cluster != 0)
		fat_clear_cache(inode->i_fs, privdata->first_cluster);

	kfree(inode->i_privdata);
	vfs_destroy_inode(inode);
}

static void
fat_fill_inode(struct VFS_INODE* inode, void* fsop, struct FAT_ENTRY* fentry)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;

	inode->i_sb.st_ino = (*(uint64_t*)fsop) & 0xffffffff; /* XXX */
	inode->i_sb.st_mode = 0755;
	inode->i_sb.st_nlink = 1;
	inode->i_sb.st_uid = 0;
	inode->i_sb.st_gid = 0;
	/* TODO */
	inode->i_sb.st_atime = 0;
	inode->i_sb.st_mtime = 0;
	inode->i_sb.st_ctime = 0;
	if (fentry != NULL) {
		inode->i_sb.st_size = FAT_FROM_LE32(fentry->fe_size);
		uint32_t cluster  = FAT_FROM_LE16(fentry->fe_cluster_lo);
		if (fs_privdata->fat_type == 32)
			cluster |= FAT_FROM_LE16(fentry->fe_cluster_hi) << 16;
		privdata->first_cluster = cluster;
		/* Distinguish between directory and inode */
		if (fentry->fe_attributes & FAT_ATTRIBUTE_DIRECTORY) {
			inode->i_iops = &fat_dir_ops;
			inode->i_sb.st_mode |= S_IFDIR;
		} else {
			inode->i_iops = &fat_inode_ops;
			inode->i_sb.st_mode |= S_IFREG;
		}
	} else {
		/* Root inode */
		inode->i_sb.st_size = fs_privdata->num_rootdir_sectors * fs_privdata->sector_size;
		inode->i_iops = &fat_dir_ops;
		inode->i_sb.st_mode |= S_IFDIR;
		if (fs_privdata->fat_type == 32)
			privdata->first_cluster = fs_privdata->first_rootdir_sector;
	}
	inode->i_sb.st_blocks = inode->i_sb.st_size / fs_privdata->sector_size;
	if (inode->i_sb.st_size % fs_privdata->sector_size)
		inode->i_sb.st_blocks++;
}

errorcode_t
fat_read_inode(struct VFS_INODE* inode, void* fsop)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;

	/*
	 * FAT doesn't really have a root inode, so we just fake one. The root_inode
	 * flag makes the read/write functions do the right thing.
	 */
	if (*(uint64_t*)fsop == FAT_ROOTINODE_FSOP) {
		privdata->root_inode = 1;
		fat_fill_inode(inode, fsop, NULL);
		return ANANAS_ERROR_NONE;
	}

	/*
	 * For ordinary inodes, the fsop is just the block with the 16-bit offset
	 * within that block.
	 */
	uint32_t block  = (*(uint64_t*)fsop) >> 16;
	uint32_t offset = (*(uint64_t*)fsop) & 0xffff;
	KASSERT(offset <= fs->fs_block_size - sizeof(struct FAT_ENTRY), "fsop inode offset %u out of range", offset);
	struct BIO* bio;
	errorcode_t err = vfs_bread(fs, block, &bio);
	ANANAS_ERROR_RETURN(err);
	/* Fill out the inode details */
	struct FAT_ENTRY* fentry = (struct FAT_ENTRY*)(void*)(BIO_DATA(bio) + offset);
	fat_fill_inode(inode, fsop, fentry);
	bio_free(bio);
	return ANANAS_ERROR_NONE;
}

errorcode_t
fat_write_inode(struct VFS_INODE* inode)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;
	void* fsop_ptr = (void*)&inode->i_fsop[0]; /* XXX kludge to avoid gcc warning */
	uint64_t fsop = *(uint64_t*)fsop_ptr;
	KASSERT(fsop != FAT_ROOTINODE_FSOP, "writing the root inode");

	/* Grab the current inode data block */
	uint32_t block  = fsop >> 16;
	uint32_t offset = fsop & 0xffff;
	KASSERT(offset <= fs->fs_block_size - sizeof(struct FAT_ENTRY), "fsop inode offset %u out of range", offset);
	struct BIO* bio;
	errorcode_t err = vfs_bread(fs, block, &bio);
	ANANAS_ERROR_RETURN(err);

	/* Fill out the inode details */
	struct FAT_ENTRY* fentry = (struct FAT_ENTRY*)(void*)(BIO_DATA(bio) + offset);
	FAT_TO_LE32(fentry->fe_size, inode->i_sb.st_size);
	FAT_TO_LE16(fentry->fe_cluster_lo, privdata->first_cluster & 0xffff);
	if (fs_privdata->fat_type == 32)
		FAT_TO_LE16(fentry->fe_cluster_hi, privdata->first_cluster >> 16);

	/* If the inode has run out of references, we must free the clusters it occupied */
	if (inode->i_sb.st_nlink == 0) {
		err = fat_truncate_clusterchain(inode);
		ANANAS_ERROR_RETURN(err); /* XXX Should this be fatal? */
	}

	/* And off it goes */
	bio_set_dirty(bio);
	bio_free(bio);
	return ANANAS_ERROR_NONE; /* XXX How should we deal with errors? */
}

struct VFS_INODE_OPS fat_inode_ops = {
	.read = vfs_generic_read,
	.write = vfs_generic_write,
	.block_map = fat_block_map
};

/* vim:set ts=2 sw=2: */
