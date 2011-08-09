/* 
 * Ananas FAT filesystem driver.
 *
 * FAT is one of the most trivial filesystems ever created - unfortunately,
 * this mostly refers to the on-disk layout, not the implementation: a lot of
 * design issues make our job actually harder.
 *
 * Disk-wise, FAT consists of the following:
 *
 * sector 0: BPB containing vital filesystem information
 * 1 to (1 + fatsize): First FAT
 * (1 + fatsize) - (1 + fatsize * numfats): Second, ... FAT
 * (1 + fatsize * numfats) to (1 + fatsize * numfats + rootdirsize): Root
 * directory entries.
 * (1 + fatsize * numfats + rootdirsize) to ... (cluster 3 to ...): Data
 * entries
 *
 * FAT works by clusters, a cluster is just a fixed amount of sectors as
 * outlined in the BPB. As neither the FAT nor the root directory are
 * cluster-aligned, there is no option but to use the smallest possible size as
 * blocksize (i.e. the sector size) - this is unfortunate, but it'll just have
 * to do.
 *
 * On a related note, FAT is basically the disk-based incarnation of a linked
 * list: ever file contains the first cluster of the data, and the FAT is used
 * to look up the next cluster, the next cluster's cluster, etc.
 *
 * Generally, everything is cluster-oriented except for the root director which
 * is stored as a consecutive list of sectors (except in FAT32, where it's just
 * a linked-list of clusters like everywhere else).
 */
#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/error.h>
#include <ananas/vfs.h>
#include <ananas/vfs/generic.h>
#include <ananas/vfs/mount.h>
#include <ananas/lib.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <ananas/init.h>
#include <fat.h>
#include "block.h"
#include "dir.h"
#include "inode.h"
#include "fatfs.h"

#undef DEBUG_FAT

TRACE_SETUP;

static errorcode_t
fat_mount(struct VFS_MOUNTED_FS* fs)
{
	/* XXX this should be made a compile-time check */
	KASSERT(sizeof(struct FAT_ENTRY) == 32, "compiler error: fat entry is not 32 bytes!");

	/* Fill out a sector size and grab the first block */
	struct BIO* bio;
	fs->fs_block_size = BIO_SECTOR_SIZE;
	errorcode_t err = vfs_bread(fs, 0, &bio);
	ANANAS_ERROR_RETURN(err);

	/* Parse what we just read */
	struct FAT_BPB* bpb = (struct FAT_BPB*)BIO_DATA(bio);
	struct FAT_FS_PRIVDATA* privdata = kmalloc(sizeof(struct FAT_FS_PRIVDATA));
	memset(privdata, 0, sizeof(struct FAT_FS_PRIVDATA));
	spinlock_init(&privdata->spl_cache);
	fs->fs_privdata = privdata; /* immediately, this is used by other functions */

#define FAT_ABORT(x...) \
	do { \
		kfree(privdata); \
		kprintf(x); \
		return ANANAS_ERROR(NO_DEVICE); \
	} while(0)

	privdata->sector_size = FAT_FROM_LE16(bpb->bpb_bytespersector);
	if (privdata->sector_size < 512)
		FAT_ABORT("sectorsize must be at least 512 (is %u)", privdata->sector_size);
	int log2_cluster_size = 0;
	for (int i = bpb->bpb_sectorspercluster; i > 1; i >>= 1, log2_cluster_size++)
		;
	if ((1 << log2_cluster_size) != bpb->bpb_sectorspercluster)
		FAT_ABORT("clustersize isn't a power of 2");
	privdata->sectors_per_cluster = bpb->bpb_sectorspercluster;
	privdata->reserved_sectors = FAT_FROM_LE16(bpb->bpb_num_reserved);
	if (privdata->reserved_sectors == 0)
		FAT_ABORT("no reserved sectors");
	privdata->num_fats = bpb->bpb_num_fats;
	if (privdata->num_fats == 0)
		FAT_ABORT("no fats on disk");
	uint16_t num_rootdir_entries = FAT_FROM_LE16(bpb->bpb_num_root_entries);
	uint32_t num_sectors = FAT_FROM_LE16(bpb->bpb_num_sectors);
	if (num_sectors == 0)
		num_sectors = FAT_FROM_LE32(bpb->bpb_large_num_sectors);
	if (num_sectors == 0)
		FAT_ABORT("no sectors on disk?");

	/*
	 * Finally, we can now figure out the FAT type in use.
	 */
	privdata->num_rootdir_sectors = ((sizeof(struct FAT_ENTRY) * num_rootdir_entries) + (privdata->sector_size - 1)) / privdata->sector_size;
	privdata->num_fat_sectors = FAT_FROM_LE16(bpb->bpb_sectors_per_fat);
	if (privdata->num_fat_sectors == 0)
		privdata->num_fat_sectors = FAT_FROM_LE32(bpb->epb.fat32.epb_sectors_per_fat);
	privdata->first_rootdir_sector = privdata->reserved_sectors + (privdata->num_fats * privdata->num_fat_sectors);
	privdata->first_data_sector = privdata->first_rootdir_sector + privdata->num_rootdir_sectors;
	privdata->total_clusters = (num_sectors - privdata->first_data_sector) / privdata->sectors_per_cluster; /* actually number of data clusters */
	if (privdata->total_clusters < 4085) {
		privdata->fat_type = 12;
	} else if (privdata->total_clusters < 65525) {
		privdata->fat_type = 16;
	} else {
		privdata->fat_type = 32;
		privdata->first_rootdir_sector = FAT_FROM_LE32(bpb->epb.fat32.epb_root_cluster);
	}
#ifdef DEBUG_FAT
	kprintf("fat: FAT%u, num_fat_sectors=%u, first_root_sector=%u, first_data_sector=%u\n",
	 privdata->fat_type, privdata->num_fat_sectors, privdata->first_rootdir_sector, privdata->first_data_sector);
#endif

	/*
	 * Reject FAT12; it's obsolete and would make the driver even more
	 * complicated than it already is.
	 */
	if (privdata->fat_type == 12)
		FAT_ABORT("FAT12 is unsupported");

	/* Everything is in order - fill out our filesystem details */
	fs->fs_block_size = privdata->sector_size;
	fs->fs_fsop_size = sizeof(uint64_t);

	/* Initialize the inode cache right before reading the root directory inode */
	icache_init(fs);

	/* Grab the root directory inode */
	uint64_t root_fsop = FAT_ROOTINODE_FSOP;
	fs->fs_root_inode = fat_alloc_inode(fs, (const void*)&root_fsop);
	err = vfs_get_inode(fs, &root_fsop, &fs->fs_root_inode);
	if (err != ANANAS_ERROR_NONE) {
		kfree(privdata);
		return err;
	}
	return ANANAS_ERROR_NONE;
}

static struct VFS_FILESYSTEM_OPS fsops_fat = {
	.mount = fat_mount,
	.alloc_inode = fat_alloc_inode,
	.destroy_inode = fat_destroy_inode,
	.read_inode = fat_read_inode,
	.write_inode = fat_write_inode
};

static struct VFS_FILESYSTEM fs_fat = {
	.fs_name = "fatfs",
	.fs_fsops = &fsops_fat
};

errorcode_t
fatfs_init()
{
	return vfs_register_filesystem(&fs_fat);
}

static errorcode_t
fatfs_exit()
{
	return vfs_unregister_filesystem(&fs_fat);
}

INIT_FUNCTION(fatfs_init, SUBSYSTEM_VFS, ORDER_MIDDLE);
EXIT_FUNCTION(fatfs_exit);

/* vim:set ts=2 sw=2: */
