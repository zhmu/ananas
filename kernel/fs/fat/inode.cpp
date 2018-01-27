#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/bio.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/vfs/types.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/generic.h"
#include "block.h"
#include "dir.h"
#include "inode.h"
#include "fat.h"
#include "fatfs.h"

errorcode_t
fat_prepare_inode(INode& inode)
{
	inode.i_privdata = new FAT_INODE_PRIVDATA;
	memset(inode.i_privdata, 0, sizeof(struct FAT_INODE_PRIVDATA));
	return ananas_success();
}

void
fat_discard_inode(INode& inode)
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

static void
fat_fill_inode(INode& inode, ino_t inum, struct FAT_ENTRY* fentry)
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
		uint32_t cluster  = FAT_FROM_LE16(fentry->fe_cluster_lo);
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

errorcode_t
fat_read_inode(INode& inode, ino_t inum)
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
		return ananas_success();
	}

	/*
	 * For ordinary inodes, the inum is just the block with the 16-bit offset
	 * within that block.
	 */
	uint32_t block  = inum >> 16;
	uint32_t offset = inum & 0xffff;
	KASSERT(offset <= fs->fs_block_size - sizeof(struct FAT_ENTRY), "inode offset %u out of range", offset);
	BIO* bio;
	errorcode_t err = vfs_bread(fs, block, &bio);
	ANANAS_ERROR_RETURN(err);
	/* Fill out the inode details */
	auto fentry = reinterpret_cast<struct FAT_ENTRY*>(static_cast<char*>(BIO_DATA(bio)) + offset);
	fat_fill_inode(inode, inum, fentry);
	bio_free(*bio);
	return ananas_success();
}

errorcode_t
fat_write_inode(INode& inode)
{
	struct VFS_MOUNTED_FS* fs = inode.i_fs;
	auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);
	auto privdata = static_cast<struct FAT_INODE_PRIVDATA*>(inode.i_privdata);
	ino_t inum = inode.i_inum;
	KASSERT(inum != FAT_ROOTINODE_INUM, "writing the root inode");

	/* Grab the current inode data block */
	uint32_t block  = inum >> 16;
	uint32_t offset = inum & 0xffff;
	KASSERT(offset <= fs->fs_block_size - sizeof(struct FAT_ENTRY), "inode offset %u out of range", offset);
	BIO* bio;
	errorcode_t err = vfs_bread(fs, block, &bio);
	ANANAS_ERROR_RETURN(err);

	/* Fill out the inode details */
	auto fentry = reinterpret_cast<struct FAT_ENTRY*>(static_cast<char*>(BIO_DATA(bio)) + offset);
	FAT_TO_LE32(fentry->fe_size, inode.i_sb.st_size);
	FAT_TO_LE16(fentry->fe_cluster_lo, privdata->first_cluster & 0xffff);
	if (fs_privdata->fat_type == 32)
		FAT_TO_LE16(fentry->fe_cluster_hi, privdata->first_cluster >> 16);

	/* If the inode has run out of references, we must free the clusters it occupied */
	if (inode.i_sb.st_nlink == 0) {
		err = fat_truncate_clusterchain(inode);
		ANANAS_ERROR_RETURN(err); /* XXX Should this be fatal? */
	}

	/* And off it goes */
	bio_set_dirty(*bio);
	bio_free(*bio);
	return ananas_success(); /* XXX How should we deal with errors? */
}

struct VFS_INODE_OPS fat_inode_ops = {
	.read = vfs_generic_read,
	.write = vfs_generic_write,
	.block_map = fat_block_map
};

/* vim:set ts=2 sw=2: */
