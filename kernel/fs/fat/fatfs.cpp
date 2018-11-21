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
#include <ananas/errno.h>
#include "kernel/bio.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/generic.h"
#include "kernel/vfs/mount.h"
#include "block.h"
#include "dir.h"
#include "fat.h"
#include "inode.h"
#include "fatfs.h"

#undef DEBUG_FAT

TRACE_SETUP;

static Result
fat_mount(struct VFS_MOUNTED_FS* fs, INode*& root_inode)
{
	/* XXX this should be made a compile-time check */
	KASSERT(sizeof(struct FAT_ENTRY) == 32, "compiler error: fat entry is not 32 bytes!");

	/* Fill out a sector size and grab the first block */
	BIO* bio;
	fs->fs_block_size = BIO_SECTOR_SIZE;
	if (auto result = vfs_bread(fs, 0, &bio); result.IsFailure())
		return result;

	/* Parse what we just read */
	struct FAT_BPB* bpb = (struct FAT_BPB*)BIO_DATA(bio);
	auto privdata = new FAT_FS_PRIVDATA;
	memset(privdata, 0, sizeof(struct FAT_FS_PRIVDATA));
	fs->fs_privdata = privdata; /* immediately, this is used by other functions */

#define FAT_ABORT(x...) \
	do { \
		kfree(privdata); \
		kprintf(x); \
		return RESULT_MAKE_FAILURE(ENODEV); \
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

	/* If we are using FAT32, there should be an 'info sector' with some (cached) information */
	privdata->num_avail_clusters = (uint32_t)-1;
	if (privdata->fat_type == 32) {
		uint16_t fsinfo_sector = FAT_FROM_LE16(bpb->epb.fat32.epb_fsinfo_sector);
		if (fsinfo_sector >= 1 && fsinfo_sector < num_sectors) {
			/* Looks sane; read it */
			BIO* bio;
			if (auto result = vfs_bread(fs, fsinfo_sector, &bio); result.IsSuccess()) {
				struct FAT_FAT32_FSINFO* fsi = (struct FAT_FAT32_FSINFO*)BIO_DATA(bio);
				if (FAT_FROM_LE32(fsi->fsi_signature1) == FAT_FSINFO_SIGNATURE1 &&
				    FAT_FROM_LE32(fsi->fsi_signature2) == FAT_FSINFO_SIGNATURE2) {
					/* File system information seems valid; use it, if sane */
					uint32_t next_free = FAT_FROM_LE32(fsi->fsi_next_free);
					if (next_free >= 2 && next_free < privdata->total_clusters)
						privdata->next_avail_cluster = next_free;
					uint32_t num_free = FAT_FROM_LE32(fsi->fsi_free_count);
					if (num_free < privdata->total_clusters)
						privdata->num_avail_clusters = num_free;

					/* We have an info sector we should update */
					privdata->infosector_num = fsinfo_sector;
				}
				bio_free(*bio);
			}
		}
	}

	if (auto result = vfs_get_inode(fs, FAT_ROOTINODE_INUM, root_inode); result.IsFailure()) {
		kfree(privdata);
		return result;
	}
	return Result::Success();
}

namespace {

struct VFS_FILESYSTEM_OPS fsops_fat = {
	.mount = fat_mount,
	.prepare_inode = fat_prepare_inode,
	.discard_inode = fat_discard_inode,
	.read_inode = fat_read_inode,
	.write_inode = fat_write_inode
};

VFSFileSystem fs_fat("fatfs", &fsops_fat);

const init::OnInit registerFATFS(init::SubSystem::VFS, init::Order::Middle, []()
{
	vfs_register_filesystem(fs_fat);
});

} // unnamed namespace

/* vim:set ts=2 sw=2: */
