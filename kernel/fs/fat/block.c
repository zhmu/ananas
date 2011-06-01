#include <ananas/types.h>
#include <ananas/vfs/types.h>
#include <ananas/vfs/core.h>
#include <ananas/vfs/generic.h>
#include <ananas/bio.h>
#include <ananas/lock.h>
#include <ananas/error.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include "fatfs.h"
#include "block.h"

TRACE_SETUP;

static inline blocknr_t
fat_cluster_to_sector(struct VFS_MOUNTED_FS* fs, uint32_t cluster)
{
	struct FAT_FS_PRIVDATA* privdata = (struct FAT_FS_PRIVDATA*)fs->fs_privdata;
	KASSERT(cluster >= 2, "invalid cluster number %u specified", cluster);

	blocknr_t ret = cluster - 2 /* really; the first 2 are reserved or something? */;
	ret *= privdata->sectors_per_cluster;
	ret += privdata->first_data_sector;
	return ret;
}

static inline void
fat_make_cluster_block_offset(struct VFS_MOUNTED_FS* fs, uint32_t cluster, blocknr_t* sector, uint32_t* offset)
{
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;

	uint32_t block;
	switch(fs_privdata->fat_type) {
		case 16:
			block = 2 * cluster;
			break;
		case 32:
			block = 4 * cluster;
			break;
		default:
			panic("fat_get_cluster(): unsupported fat size %u", fs_privdata->fat_type);
	}
	*sector = fs_privdata->reserved_sectors + (block / fs_privdata->sector_size);
	*offset = block % fs_privdata->sector_size;
}

/*
 * Used to obtain the clusternum'th cluster of a file starting at
 * first_cluster. Returns BAD_RANGE error if end-of-file was found (but
 * cluster_out will be set to the final cluster found_
 */
static errorcode_t
fat_get_cluster(struct VFS_MOUNTED_FS* fs, uint32_t first_cluster, uint32_t clusternum, uint32_t* cluster_out)
{
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;
	uint32_t cur_cluster = first_cluster;

	/*
	 * First of all, look through our cache. XXX linear search.
	 *
	 * We assume the cache items will never be removed while we are working with
	 * the file because it will remain referenced - we throw away the cache items
	 * when the inode is freed.
	 *
	 * Note that we explicitely _do not_ cache cluster -1; it is used to find the
	 * final cluster of the FAT chain.
	 */
try_cache: ; /* dummy ; to keep gcc happy */
	int create = 0, found = 0;
	struct FAT_CLUSTER_CACHEITEM* ci = NULL;
	struct FAT_CLUSTER_CACHEITEM* ci_closest = NULL;
	if (clusternum != -1) {
		spinlock_lock(&fs_privdata->spl_cache);
		for(int cache_item = 0; cache_item < FAT_NUM_CACHEITEMS; cache_item++) {
			ci = &fs_privdata->cluster_cache[cache_item];
			if (ci->f_clusterno == 0) {
				/* Found empty item - use it (we assume this always occurs at the end of the items) */
				create++;
				ci->f_clusterno = first_cluster;
				ci->f_index = clusternum;
				ci->f_nextcluster = 0;
				break;
			}
			if (ci->f_clusterno == first_cluster) {
				if (ci->f_index == clusternum) {
					/* Got it */
					found++;
					break;
				}

				/*
				 * Cluster is fine, but index isn't. Attempt to use this item as a base;
				 * the less items we'll have to walk through, the better.
				 */
				if (ci->f_index < clusternum && (ci_closest == NULL || ci->f_index > ci_closest->f_index))
					ci_closest = ci;
			}
		}
		/*
		 * If we need to do a lookup, use the closest item as base; we do this
		 * while holding the lock to ensure the item won't vanish.
		 */
		if (!found && ci_closest != NULL) {
			cur_cluster = ci_closest->f_nextcluster;
			clusternum -= ci_closest->f_index;
		}
		spinlock_unlock(&fs_privdata->spl_cache);

		if (create == 0 && found == 0) {
			/*
			 * Out of cache items; we'll just throw away all cached clusters of this
			 * item. If this doesn't work, we'll just overwrite the first item XXX
			 */
			if (fat_clear_cache(fs, first_cluster))
				goto try_cache;
			ci = &fs_privdata->cluster_cache[0];
			ci->f_clusterno = first_cluster;
			ci->f_index = clusternum;
			ci->f_nextcluster = 0;
			create++;
			kprintf("fat_get_cluster(): sacrificed first cache block\n");
		}
	}

	/*
	 * If we are not creating, yet the next cluster value is zero, someone else is
	 * looking up the item, we need to spin.
	 */
	if (ci != NULL && create == 0) {
		while (ci->f_nextcluster == 0) {
			kprintf("fat_get_cluster(): pending cluster found, waiting...\n");
			reschedule();
		}
		if (ci->f_nextcluster == -1)
			return ANANAS_ERROR(BAD_RANGE);
		*cluster_out = ci->f_nextcluster;
		return ANANAS_ERROR_NONE;
	}

	/* Not in the cache; we'll need to traverse the disk */
	*cluster_out = first_cluster;
	while (cur_cluster != 0 && clusternum-- > 0) {
		blocknr_t sector_num;
		uint32_t offset;
		fat_make_cluster_block_offset(fs, cur_cluster, &sector_num, &offset);
		struct BIO* bio;
		errorcode_t err = vfs_bread(fs, sector_num, &bio);
		ANANAS_ERROR_RETURN(err);

		/* Grab the value from the FAT */
		switch (fs_privdata->fat_type) {
			case 16:
				cur_cluster = FAT_FROM_LE16((char*)(BIO_DATA(bio) + offset));
				if (cur_cluster >= 0xfff8)
					cur_cluster = 0;
				break;
			case 32: /* actually FAT-28... */
				cur_cluster = FAT_FROM_LE32((char*)(BIO_DATA(bio) + offset)) & 0xfffffff;
				if (cur_cluster >= 0xffffff8)
					cur_cluster = 0;
				break;
		}
		bio_free(bio);
		if (cur_cluster != 0)
			*cluster_out = cur_cluster;
	}
	if (cur_cluster != 0) {
		if (ci != NULL)
			ci->f_nextcluster = *cluster_out;
		return ANANAS_ERROR_OK;
	} else {
		if (ci != NULL)
			ci->f_nextcluster = -1;
		return ANANAS_ERROR(BAD_RANGE);
	}
}

/*
 * Sets a cluster value to a given value.
 */
static errorcode_t
fat_set_cluster(struct VFS_MOUNTED_FS* fs, uint32_t cluster_num, uint32_t cluster_val)
{
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;

	/* Calculate the block and offset within that block of the cluster */
	blocknr_t sector_num;
	uint32_t offset;
	fat_make_cluster_block_offset(fs, cluster_num, &sector_num, &offset);

	/* Fetch the FAT data */
	struct BIO* bio;
	errorcode_t err = vfs_bread(fs, sector_num, &bio);
	ANANAS_ERROR_RETURN(err);

	switch (fs_privdata->fat_type) {
		case 16:
			FAT_TO_LE16((char*)(BIO_DATA(bio) + offset), cluster_val);
			break;
		case 32: /* actually FAT-28... */
			FAT_TO_LE32((char*)(BIO_DATA(bio) + offset), cluster_val);
			break;
		default:
			panic("unsuported fat type");
	}

	bio_set_dirty(bio);

	/* Sync all other FAT tables as well */
	for (int i = 1; i < fs_privdata->num_fats; i++) {
		sector_num += fs_privdata->num_fat_sectors;
		struct BIO* bio2;
		err = vfs_bread(fs, sector_num, &bio2);
		if (err != ANANAS_ERROR_NONE) {
			/* XXX we should free the cluster */
			bio_free(bio);
			kprintf("fat_set_cluster(): XXX leaking cluster %u\n", sector_num);
			return err;
		}
		memcpy(BIO_DATA(bio2), BIO_DATA(bio), fs_privdata->sector_size);
		bio_set_dirty(bio2);
		bio_free(bio2);
	}
	bio_free(bio);

	return ANANAS_ERROR_OK;
}

/*
 * Obtains the first available cluster and marks it as being used.
 */
static errorcode_t
fat_claim_avail_cluster(struct VFS_MOUNTED_FS* fs, uint32_t* cluster_out)
{
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;

	/* XXX We do a dumb find-first on the filesystem for now */
	blocknr_t cur_block;
	struct BIO* bio = NULL;
	for (uint32_t clusterno = fs_privdata->next_avail_cluster; clusterno < fs_privdata->total_clusters; clusterno++) {
		/* Obtain the FAT block, if necessary */
		uint32_t offset;
		blocknr_t want_block;
		fat_make_cluster_block_offset(fs, clusterno, &want_block, &offset);
		if (want_block != cur_block || bio == NULL) {
			if (bio != NULL) bio_free(bio);
			errorcode_t err = vfs_bread(fs, want_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		/* Obtain the cluster value */
		uint32_t val = -1;
		switch (fs_privdata->fat_type) {
			case 16:
				val = FAT_FROM_LE16((char*)(BIO_DATA(bio) + offset));
				if (val == 0) { /* available cluster? */
					/* Yes; claim it */
					FAT_TO_LE16((char*)(BIO_DATA(bio) + offset), 0xfff8);
				}
				break;
			case 32: /* actually FAT-28... */
				val = FAT_FROM_LE32((char*)(BIO_DATA(bio) + offset)) & 0xfffffff;
				if (val == 0) { /* available cluster? */
					/* Yes; claim it */
					FAT_TO_LE32((char*)(BIO_DATA(bio) + offset), 0xffffff8);
				}
				break;
		}
		if (val != 0)
			continue;

		/* OK; this one is available (and we just claimed it) - flush the FAT entry */
		bio_set_dirty(bio);
		bio_free(bio);

		/* XXX should update second fat */
		*cluster_out = clusterno;
		return ANANAS_ERROR_NONE;
	}

	/* Out of available clusters */
	return ANANAS_ERROR(NO_SPACE);
}

/*
 * Appends a new cluster to an inode; returns the next cluster.
 */
static errorcode_t
fat_append_cluster(struct VFS_INODE* inode, uint32_t* cluster_out)
{
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;

	/*
	 * Figure out the last cluster of the file; we cache this per inode as we don't
	 * want to clutter the cluster-cache with it (these entries need to be updated
	 * if we append a cluster)
	 */
	uint32_t last_cluster = privdata->last_cluster;
	if (last_cluster == 0) {
		errorcode_t err = fat_get_cluster(fs, privdata->first_cluster, (uint32_t)-1, &last_cluster);
		if (ANANAS_ERROR_CODE(err) != ANANAS_ERROR_BAD_RANGE) {
			KASSERT(err != ANANAS_ERROR_OK, "able to obtain impossible cluster");
			return err;
		}
		privdata->last_cluster = last_cluster;
	}

	/* Obtain the next cluster - this will also mark it as being in use */
	uint32_t new_cluster = 0;
	errorcode_t err = fat_claim_avail_cluster(fs, &new_cluster);
	ANANAS_ERROR_RETURN(err);

	/* If the file didn't have any clusters before, it sure does now */
	if (privdata->first_cluster == 0) {
		privdata->first_cluster = new_cluster;
		vfs_set_inode_dirty(inode);
	} else {
		/* Append this cluster to the file chain */
		err = fat_set_cluster(fs, last_cluster, new_cluster);
		ANANAS_ERROR_RETURN(err); /* XXX leaks clusterno */
	}
	*cluster_out = new_cluster;

	/*
	 * Update the cache; if there is an entry for our inode which is marked as
	 * nonexistent, we expect it to be the last one and overwrite it.
	 */
	spinlock_lock(&fs_privdata->spl_cache);
	for(int cache_item = 0; cache_item < FAT_NUM_CACHEITEMS; cache_item++) {
		struct FAT_CLUSTER_CACHEITEM* ci = &fs_privdata->cluster_cache[cache_item];
		if (ci->f_clusterno == privdata->first_cluster && ci->f_nextcluster == -1) {
			/* Found empty item - use it (we assume this always occurs at the end of the items) */
			KASSERT(ci->f_index == (inode->i_sb.st_size + ((fs_privdata->sector_size * fs_privdata->sectors_per_cluster) - 1)) / (fs_privdata->sector_size * fs_privdata->sectors_per_cluster), "empty cache item isn't final item?");
			ci->f_nextcluster = new_cluster;
			break;
		}
	}
	spinlock_unlock(&fs_privdata->spl_cache);

	/* Update the block count of the inode */
	privdata->last_cluster = new_cluster;
	inode->i_sb.st_blocks += fs_privdata->sectors_per_cluster;
	return ANANAS_ERROR_OK;
}

/*
 * Maps the given block of an inode to the block device's block to use; note
 * that in our FAT implementation, a block size is identical to a sector size
 * (this is necessary because the data may not begin at a sector number
 * multiple of a cluster size)
 */
errorcode_t
fat_block_map(struct VFS_INODE* inode, blocknr_t block_in, blocknr_t* block_out, int create)
{
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;

	/*
	 * FAT16 root inodes are special as they have a fixed location and
	 * length; the FAT isn't needed to traverse them.
	 */
	blocknr_t want_block;
	if (privdata->root_inode && fs_privdata->fat_type != 32) {
		want_block = fs_privdata->first_rootdir_sector + block_in;
		if (block_in >= (fs_privdata->num_rootdir_sectors * fs_privdata->sector_size))
			/* Cannot expand the root directory */
			return ANANAS_ERROR(BAD_RANGE);
	} else {
		uint32_t cluster;
		errorcode_t err = fat_get_cluster(fs, privdata->first_cluster, block_in / fs_privdata->sectors_per_cluster, &cluster);
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_BAD_RANGE) {
			/* end of the chain */
			if (!create) {
				return err;
			}
			err = fat_append_cluster(inode, &cluster);
			ANANAS_ERROR_RETURN(err);
		} else if (err == ANANAS_ERROR_NONE) {
			KASSERT(create == 0, "request to create block that already exists (blocknum=%u, cluster=%u)", (int)(block_in / fs_privdata->sectors_per_cluster), cluster);
		} else {
			return err;
		}

		want_block  = fat_cluster_to_sector(fs, cluster);
		want_block += block_in % fs_privdata->sectors_per_cluster;
	}

	*block_out = want_block;
	return ANANAS_ERROR_OK;
}

int
fat_clear_cache(struct VFS_MOUNTED_FS* fs, uint32_t first_cluster)
{
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;

	/*
	 * Throw away al inode's cluster items; this works by searching for the last
	 * item in the cluster cache and copying it over our removed blocks - this
	 * works because the cache isn't sorted.
	 */
	spinlock_lock(&fs_privdata->spl_cache);
	unsigned int last_index = 0;
	while (last_index < FAT_NUM_CACHEITEMS && fs_privdata->cluster_cache[last_index].f_clusterno != 0)
		last_index++;
	/* Cacheitems 0 ... i, for all i < last_index are in use */

	/* Throw away all inode's cluster items in the cache */
	int num_removed = 0;
	struct FAT_CLUSTER_CACHEITEM* ci = fs_privdata->cluster_cache;
	for(unsigned int n = 0; n < FAT_NUM_CACHEITEMS; n++, ci++) {
		if (ci->f_clusterno != first_cluster)
			continue;

		/*
		 * OK, this is an item we'll have to destroy - copy the last entry over it and remove that one.
		 * If this is the final entry, we skip the copying.
			*/
		KASSERT(last_index >= 0 && last_index <= FAT_NUM_CACHEITEMS, "last index %u invalid", last_index);
		KASSERT(n <= last_index, "item to remove %u is beyond last index %i", n, last_index);
		if (n != (last_index - 1)) {
			/* Need to overwrite this item with the final cache item */
			memcpy(ci, &fs_privdata->cluster_cache[last_index - 1], sizeof(*ci));
			memset(&fs_privdata->cluster_cache[last_index - 1], 0, sizeof(*ci));
			last_index--;
		} else {
			memset(ci, 0, sizeof(*ci));
		}
		num_removed++;
	}
	spinlock_unlock(&fs_privdata->spl_cache);
	return num_removed;
}

void
fat_dump_cache(struct VFS_MOUNTED_FS* fs)
{
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;
	struct FAT_CLUSTER_CACHEITEM* ci = fs_privdata->cluster_cache;
	for(unsigned int n = 0; n < FAT_NUM_CACHEITEMS; n++, ci++) {
		kprintf("ci[%i]: clusterno=%i, index=%i, nextcl=%u\n",
		 n, ci->f_clusterno, ci->f_index, ci->f_nextcluster);
	}
}

/* vim:set ts=2 sw=2: */