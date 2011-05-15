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
#include <ananas/lib.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <fat.h>

#undef DEBUG_FAT

#define FAT_FROM_LE16(x) (((uint8_t*)(x))[0] | (((uint8_t*)(x))[1] << 8))
#define FAT_FROM_LE32(x) (((uint8_t*)(x))[0] | (((uint8_t*)(x))[1] << 8) | (((uint8_t*)(x))[2] << 16) | (((uint8_t*)(x))[3] << 24))

#define FAT_TO_LE16(x, y) do { \
		((uint8_t*)(x))[0] =  (y)       & 0xff; \
		((uint8_t*)(x))[1] = ((y) >> 8) & 0xff; \
	} while(0)

#define FAT_TO_LE32(x, y) do { \
		((uint8_t*)(x))[0] =  (y)        & 0xff; \
		((uint8_t*)(x))[1] = ((y) >>  8) & 0xff; \
		((uint8_t*)(x))[2] = ((y) >> 16) & 0xff; \
		((uint8_t*)(x))[3] = ((y) >> 24) & 0xff; \
	} while(0)

/* Number of cache items per filesystem */
#define FAT_NUM_CACHEITEMS	1000

struct FAT_CLUSTER_CACHEITEM {
	uint32_t	f_clusterno;
	uint32_t	f_index;
	uint32_t	f_nextcluster;
};

struct FAT_FS_PRIVDATA {
	int fat_type;                     /* FAT type: 12, 16 or 32 */
	int	sector_size;                  /* Sector size, in bytes */
	uint32_t sectors_per_cluster;			/* Cluster size (in sectors) */
	uint16_t reserved_sectors;        /* Number of reserved sectors */
	int num_fats;                     /* Number of FATs on disk */
	int num_rootdir_sectors;          /* Number of sectors occupying root dir */
	uint32_t first_rootdir_sector;    /* First sector containing root dir */
	uint32_t first_data_sector;       /* First sector containing file data */
	uint32_t total_clusters;          /* Total number of clusters on filesystem */
	uint32_t next_avail_cluster;      /* Next available cluster */
	struct SPINLOCK spl_cache;
	struct FAT_CLUSTER_CACHEITEM cluster_cache[FAT_NUM_CACHEITEMS];
};

struct FAT_INODE_PRIVDATA {
	int root_inode;
	uint32_t first_cluster;
};

/*
 * Used to uniquely identify a FAT12/16 root inode; it appears on a
 * special, fixed place and has a limited number of entries. FAT32
 * just uses a normal cluster as FAT12/16 do for subdirectories.
 */
#define FAT_ROOTINODE_FSOP 0xfffffffe

TRACE_SETUP;

static void
fat_dump_cache(struct VFS_MOUNTED_FS* fs)
{
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;
	struct FAT_CLUSTER_CACHEITEM* ci = fs_privdata->cluster_cache;
	for(unsigned int n = 0; n < FAT_NUM_CACHEITEMS; n++, ci++) {
		kprintf("ci[%i]: clusterno=%i, index=%i, nextcl=%u\n",
		 n, ci->f_clusterno, ci->f_index, ci->f_nextcluster);
	}
}

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

static struct VFS_INODE*
fat_alloc_inode(struct VFS_MOUNTED_FS* fs, const void* fsop)
{
	struct VFS_INODE* inode = vfs_make_inode(fs, fsop);
	if (inode == NULL)
		return NULL;
	inode->i_privdata = kmalloc(sizeof(struct FAT_INODE_PRIVDATA));
	memset(inode->i_privdata, 0, sizeof(struct FAT_INODE_PRIVDATA));
	return inode;
}

static void
fat_destroy_inode(struct VFS_INODE* inode)
{
	struct FAT_FS_PRIVDATA* fs_privdata = inode->i_fs->fs_privdata;
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;

	/*
	 * If the file has backing storage, we need to throw away all inode's cluster
	 * items in the cache; this works by searching for the last item in the
	 * cluster cache and copying it over our removed blocks - this works because
	 * the cache isn't sorted.
	 */
	if (privdata->first_cluster != 0) {
		spinlock_lock(&fs_privdata->spl_cache);
		unsigned int last_index = 0;
		while (last_index < FAT_NUM_CACHEITEMS && fs_privdata->cluster_cache[last_index].f_clusterno != 0)
			last_index++;
		/* Cacheitems 0 ... i, for all i < last_index are in use */

		/* Throw away all inode's cluster items in the cache */
		struct FAT_CLUSTER_CACHEITEM* ci = fs_privdata->cluster_cache;
		for(unsigned int n = 0; n < FAT_NUM_CACHEITEMS; n++, ci++) {
			if (ci->f_clusterno != privdata->first_cluster)
				continue;

			/*
			 * OK, this is an item we'll have to destroy - copy the last entry over it and remove that one.
			 * If this is the final entry, we skip the copying.
				*/
			KASSERT(last_index >= 0 && last_index <= FAT_NUM_CACHEITEMS, "last index %u invalid", last_index);
			if (n > last_index)
				fat_dump_cache(inode->i_fs);
			KASSERT(n <= last_index, "item to remove %u is beyond last index %i", n, last_index);
			if (n != (last_index - 1)) {
				/* Need to overwrite this item with the final cache item */
				memcpy(ci, &fs_privdata->cluster_cache[last_index - 1], sizeof(*ci));
				memset(&fs_privdata->cluster_cache[last_index - 1], 0, sizeof(*ci));
				last_index--;
			} else {
				memset(ci, 0, sizeof(*ci));
			}
		}
		spinlock_unlock(&fs_privdata->spl_cache);
	}

	kfree(inode->i_privdata);
	vfs_destroy_inode(inode);
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
		case 12:
			block = cluster + (cluster / 2); /* = 1.5 * cluster */
			/* XXX FALLTHROUGH for now - write will fail */
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
	 */
	int create = 0, found = 0;
	struct FAT_CLUSTER_CACHEITEM* ci = NULL;
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
		if (ci->f_clusterno == first_cluster && ci->f_index == clusternum) {
			/* Got it */
			found++;
			break;
		}
	}
	spinlock_unlock(&fs_privdata->spl_cache);
	if (create == 0 && found == 0) {
		ci = NULL;
		kprintf("fat_get_cluster(): out of cache items\n");
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
			case 12: {
				/*
				 * FAT12 really sucks; a FAT entry may span over the border of the sector.
				 * We'll have to be careful and check for this case...
				 */
				uint16_t fat_value;
				if (offset == (fs_privdata->sector_size - 1)) {
					struct BIO* bio2;
					err = vfs_bread(fs, sector_num + 1, &bio2);
					if (err != ANANAS_ERROR_OK) {
						bio_free(bio);
						if (ci != NULL)
							ci->f_nextcluster = -1;
						return err;
					}
					fat_value  = *(uint8_t*)(BIO_DATA(bio) + offset);
					fat_value |= *(uint8_t*)(BIO_DATA(bio2)) << 8;
					bio_free(bio2);
				} else {
					fat_value = FAT_FROM_LE16((char*)(BIO_DATA(bio) + offset));
				}
				if (cur_cluster & 1) {
					fat_value >>= 4;
				} else {
					fat_value &= 0x0fff;
				}
				cur_cluster = fat_value;
				if (cur_cluster >= 0xff8)
					cur_cluster = 0;
				break;
			}
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
 * Sets a cluster value to a given value. If *bio isn't NULL, it's assumed to be
 * the buffer containing the cluster data - otherwise it will be updated to
 * contain just that.
 */
static errorcode_t
fat_set_cluster(struct VFS_MOUNTED_FS* fs, struct BIO** bio, uint32_t cluster_num, uint32_t cluster_val)
{
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;

	/* Calculate the block and offset within that block of the cluster */
	blocknr_t sector_num;
	uint32_t offset;
	fat_make_cluster_block_offset(fs, cluster_num, &sector_num, &offset);

	/* If a bio buffer isn't given, read it */
	if (*bio == NULL) {
		errorcode_t err = vfs_bread(fs, sector_num, bio);
		ANANAS_ERROR_RETURN(err);
	}

	switch (fs_privdata->fat_type) {
		case 12:
			panic("writeme: fat12 support");
		case 16:
			FAT_TO_LE16((char*)(BIO_DATA(*bio) + offset), cluster_val);
			break;
		case 32: /* actually FAT-28... */
			FAT_TO_LE32((char*)(BIO_DATA(*bio) + offset), cluster_val);
			break;
		default:
			panic("unsuported fat type");
	}

	bio_set_dirty(*bio);
	return ANANAS_ERROR_OK;
}

/*
 * 	Appends a new cluster to an inode; returns the next cluster.
 */
static errorcode_t
fat_append_cluster(struct VFS_INODE* inode, uint32_t* cluster_out)
{
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;

	/* Figure out the last cluster of the file */
	uint32_t last_cluster;
	errorcode_t err = fat_get_cluster(fs, privdata->first_cluster, (uint32_t)-1, &last_cluster);
	if (ANANAS_ERROR_CODE(err) != ANANAS_ERROR_BAD_RANGE) {
		KASSERT(err != ANANAS_ERROR_OK, "able to obtain impossible cluster");
		return err;
	}

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
			case 12:
				panic("writeme: fat12 support");
			case 16:
				val = FAT_FROM_LE16((char*)(BIO_DATA(bio) + offset));
				if (val == 0)
					FAT_TO_LE16((char*)(BIO_DATA(bio) + offset), 0xfff8);
				break;
			case 32: /* actually FAT-28... */
				val = FAT_FROM_LE32((char*)(BIO_DATA(bio) + offset)) & 0xfffffff;
				if (val == 0)
					FAT_TO_LE32((char*)(BIO_DATA(bio) + offset), 0xffffff8);
				break;
		}
		if (val != 0)
			continue;

		/* OK; this one is available (and we just claimed it) */
		bio_set_dirty(bio);
		bio_free(bio);

		/* If the file didn't have any clusters before, it sure does now */
		if (privdata->first_cluster == 0) {
			privdata->first_cluster = clusterno;
			vfs_set_inode_dirty(inode);
		} else {
			/* Append this cluster to the chain */
			bio = NULL;
			err = fat_set_cluster(fs, &bio, last_cluster, clusterno);
			ANANAS_ERROR_RETURN(err); /* XXX leaks clusterno */
		}
		*cluster_out = clusterno;
		return ANANAS_ERROR_OK;
	}

	/* out of space */
	panic("out of space (deal with me)");
}

/*
 * Maps the given block of an inode to the block device's block to use; note
 * that on FAT, a block size is identical to a sector size.
 */
static errorcode_t
fat_block_map(struct VFS_INODE* inode, blocknr_t block_in, blocknr_t* block_out, int create)
{
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;
	struct BIO* bio = NULL;

	/*
	 * FAT12/16 root inodes are special as they have a fixed location and
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
			if (!create)
				return err;
			err = fat_append_cluster(inode, &cluster);
		}
		ANANAS_ERROR_RETURN(err);

		want_block  = fat_cluster_to_sector(fs, cluster);
		want_block += block_in % fs_privdata->sectors_per_cluster;
	}

	*block_out = want_block;
	return ANANAS_ERROR_OK;
}

static struct VFS_INODE_OPS fat_file_ops = {
	.read = vfs_generic_read,
	.write  = vfs_generic_write,
	.block_map = fat_block_map
};

#if 0
static void
fat_dump_entry(struct FAT_ENTRY* fentry)
{
	kprintf("filename   '");
	for(int i = 0; i < 11; i++)
		kprintf("%c", fentry->fe_filename[i]);
	kprintf("'\n");
	kprintf("attributes 0x%x\n", fentry->fe_attributes);
	uint32_t cluster = FAT_FROM_LE16(fentry->fe_cluster_lo);
	cluster |= FAT_FROM_LE16(fentry->fe_cluster_hi) << 16; /* XXX FAT32 only */
	kprintf("cluster    %u\n", cluster);
	kprintf("size       %u\n", FAT_FROM_LE32(fentry->fe_size));
}
#endif

/*
 * Used to construct a FAT filename. Returns zero if the filename is not
 * complete.
 */
static int
fat_construct_filename(struct FAT_ENTRY* fentry, char* fat_filename)
{
#define ADD_CHAR(c) do { \
		fat_filename[pos] = (c); \
		pos++; \
	} while(0)

	int pos = 0;
	if (fentry->fe_attributes == FAT_ATTRIBUTE_LFN) {
		struct FAT_ENTRY_LFN* lfnentry = (struct FAT_ENTRY_LFN*)fentry;

    /* LFN entries needn't be sequential, so fill them out as they pass by */
    pos = ((lfnentry->lfn_order & ~LFN_ORDER_LAST) - 1) * 13;

		/* LFN XXX should we bother about the checksum? */
		/* XXX we throw away the upper 8 bits */
		for (int i = 0; i < 5; i++)
			ADD_CHAR(lfnentry->lfn_name_1[i * 2]);
		for (int i = 0; i < 6; i++)
			ADD_CHAR(lfnentry->lfn_name_2[i * 2]);
		for (int i = 0; i < 2; i++)
			ADD_CHAR(lfnentry->lfn_name_3[i * 2]);
		return 0;
	}

	/*
	 * If we have a current LFN entry, we should use that instead and
	 * ignore the old 8.3 filename.
	 */
	if (fat_filename[0] != '\0')
			return 1;

	/* Convert the filename bits */
	for (int i = 0; i < 11; i++) {
		unsigned char ch = fentry->fe_filename[i];
		if (ch == 0x20)
			continue;
		if (i == 8) /* insert a '.' after the first 8 chars */
			ADD_CHAR('.');
		if (ch == 0x05) /* kanjii allows 0xe5 in filenames, so convert */
			ch = 0xe5;
		ADD_CHAR(ch);
	}

#undef ADD_CHAR

	return 1;
}

static errorcode_t
fat_readdir(struct VFS_FILE* file, void* dirents, size_t* len)
{
	struct VFS_MOUNTED_FS* fs = file->f_inode->i_fs;
	size_t written = 0;
	size_t left = *len;
	char cur_filename[128]; /* currently assembled filename */
	int cur_filename_len = 0;
	off_t full_filename_offset = file->f_offset;
	struct BIO* bio = NULL;
	errorcode_t err = ANANAS_ERROR_OK;

	memset(cur_filename, 0, sizeof(cur_filename));

	blocknr_t cur_block;
	while(left > 0) {
		/* Obtain the current directory block data */
		blocknr_t want_block;
		errorcode_t err = fat_block_map(file->f_inode, (file->f_offset / (blocknr_t)fs->fs_block_size), &want_block, 0);
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_BAD_RANGE)
			break;
		ANANAS_ERROR_RETURN(err);
		if (want_block != cur_block || bio == NULL) {
			if (bio != NULL) bio_free(bio);
			err = vfs_bread(fs, want_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		uint32_t cur_offs = file->f_offset % fs->fs_block_size;
		file->f_offset += sizeof(struct FAT_ENTRY);
		struct FAT_ENTRY* fentry = BIO_DATA(bio) + cur_offs;
		if (fentry->fe_filename[0] == '\0') {
			/* First charachter is nul - this means there are no more entries to come */
			break;
		}

		if (fentry->fe_filename[0] == 0xe5) {
			/* Deleted file; we'll skip it */
			continue;
		}

		/* Convert the filename bits */
		if (!fat_construct_filename(fentry, cur_filename)) {
			/* This is part of a long file entry name - get more */
			continue;
		}

		/*
		 * If this is a volume entry, ignore it (but do this after the LFN has been handled
		 * because these will always have the volume bit set)
		 */
		if (fentry->fe_attributes & FAT_ATTRIBUTE_VOLUMEID)
			continue;

		/* And hand it to the fill function */
		uint64_t fsop = cur_block << 16 | cur_offs;
		int filled = vfs_filldirent(&dirents, &left, (const void*)&fsop, fs->fs_fsop_size, cur_filename, strlen(cur_filename));
		if (!filled) {
			/*
			 * Out of space - we need to restore the offset of the LFN chain. This must
			 * be done because we have assembled a full filename, only to find out that
			 * it does not fit; we need to do all this work again for the next readdir
		 	 * call.
			 */
			file->f_offset = full_filename_offset;
			break;
		}
		written += filled; cur_filename_len = 0;
		/*
		 * Store the next offset; this is where our next filename starts (which
	 	 * does not have to fit in the destination buffer, so we'll have to
		 * read everything again)
		 */
		full_filename_offset = file->f_offset;

		/* Start over from the next filename */
		memset(cur_filename, 0, sizeof(cur_filename));
	}
	if (bio != NULL) bio_free(bio);
	*len = written;
	return err;
}

/*
 * Construct the 8.3-FAT notation for a given shortname and calculates the
 * checksum while doing so.
 */
static uint8_t
fat_sanitize_83_name(const char* fname, char* shortname)
{
	/* First, convert the filename to 8.3 form */
	int shortname_idx = 0;
	memset(shortname, ' ', 11);
	KASSERT(strlen(fname) <= 11, "shortname not short enough");
	for (int i = 0; i < strlen(fname); i++) {
		if (fname[i] == '.') {
			KASSERT(shortname_idx < 8, "multiple dots in shortname");
			shortname_idx = 8;
			continue;
		}
		shortname[shortname_idx++] = fname[i];
	}

	/* Calculate the checksum for LFN entries */
	uint8_t a = 0;
	for (int i = 0; i < 11; i++) {
		a  = ((a & 1) ? 0x80 : 0) | (a >> 1); /* a = a ror 1 */
		a += shortname[i];
	}
	return a;
}

/*
 * Adds a given FAT entry to a directory.
 *
 * This will update the name of the entry given. If dentry is NULL, no LFN name
 * will be generated.
 *
 * Note that we use uint32_t's because these are quicker to use and a directory
 * size cannot be >4GB anyway (MS docs claim most implementations use an
 * uint16_t!) - so relying on large FAT directories would be most unwise anyway.
 */
static errorcode_t
fat_add_directory_entry(struct VFS_INODE* dir, const char* dentry, struct FAT_ENTRY* fentry, void* fsop)
{
	struct VFS_MOUNTED_FS* fs = dir->i_fs;
	size_t written = 0;
	struct BIO* bio = NULL;
	errorcode_t err = ANANAS_ERROR_OK;

	/*
	 * Calculate the number of entries we need; each LFN entry stores 13 characters,
	 * and we'll need an entry for the file itself too. Note that we ALWAYS store
	 * a LFN name if a name is given, even if the 8.3 name would be sufficient.
	 */
	int chain_needed = 1;
	if (dentry != NULL)
		chain_needed += (strlen(dentry) + 12) / 13;

	blocknr_t cur_block;
	uint32_t current_filename_offset = (uint32_t)-1;
	int	cur_lfn_chain = 0;
	uint32_t cur_dir_offset = 0;
	while(1) {
		/* Obtain the current directory block data */
		blocknr_t want_block;
		errorcode_t err = fat_block_map(dir, (cur_dir_offset / (blocknr_t)fs->fs_block_size), &want_block, 0);
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_BAD_RANGE) {
			/* We've hit an end-of-file - this means we'll have to enlarge the directory */
			cur_lfn_chain = -1;
			break;
		}
		ANANAS_ERROR_RETURN(err);
		if (want_block != cur_block || bio == NULL) {
			if (bio != NULL) bio_free(bio);
			err = vfs_bread(fs, want_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		/* See what the entry is all about */
		uint32_t cur_offs = cur_dir_offset % fs->fs_block_size;
		struct FAT_ENTRY* fentry = BIO_DATA(bio) + cur_offs;
		if (fentry->fe_filename[0] == '\0') {
			/*
			 * First charachter is nul - this means there are no more entries to come
			 * and we can just write our entry here. Note that we assume there are no
			 * orphanaged LFN entries before here, which should be the case anyway
			 * they should be removed already)
		 	 */
			current_filename_offset = cur_offs;
			cur_lfn_chain = 0;
			break;
		}

		if (fentry->fe_attributes == FAT_ATTRIBUTE_LFN) {
			/*
			 * This entry is part of the LFN chain; store the offset if it's the
			 * first one as we may chose to re-use the entire chain.
			 */
			if (cur_lfn_chain++ == 0)
				current_filename_offset = cur_offs;
			cur_dir_offset += sizeof(struct FAT_ENTRY);
			continue;
		}

		if (fentry->fe_filename[0] == 0xe5 && cur_lfn_chain >= chain_needed) {
			/*
			 * Entry here is deleted; this should come at the end of a LFN chain. If there
			 * is no chain, the check above will fail regardless and we skip this entry.
			  */
			break;
		}

		/*
		 * This isn't a LFN entry, nor is it a removed entry; we must skip over it.
		 */
		current_filename_offset = cur_offs;
		cur_lfn_chain = 0;
		cur_dir_offset += sizeof(struct FAT_ENTRY);
	}

	/*
	 * With LFN, shortnames are actually not used so we can stash whatever we
	 * want there; it will only show up in tools that do not support LFN, which
	 * we truly don't care about. So instead of using MS' patented(!) shortname
	 * generation schema, we just roll our own - all that matters is that it's
	 * unique in the directory, so we use the offset.
	 */
	uint8_t shortname_checksum;
	if (dentry != NULL) {
		char tmp_fname[12];
		sprintf(tmp_fname, "%u.LFN", current_filename_offset);
		shortname_checksum = fat_sanitize_83_name(tmp_fname, fentry->fe_filename);
	} /* else if (dentry == NULL) ... nothing to do, cur_entry_idx cannot be != chain_needed-1 */

	/*
	 * We've fallen out of our loop; there are three possible conditions (errors
	 * are handled directly and will not cause us to end up here):
	 *
	 * 1) cur_lfn_chain < 0  => Out of directory entries
	 *    The directory is to be enlarged and we can use its first entry.
	 * 2) cur_lfn_chain == 0 => End of directory
	 *    We can add the entry at cur_dir_offset
	 * 3) cur_lfn_chain > 0  => We've found entries we can overwrite
	 *    If this condition hits, there is enough space to place our entries.
	 */
	int filename_len = strlen(dentry);
	for(int cur_entry_idx = 0; cur_entry_idx < chain_needed; cur_entry_idx++) {
		/* Fetch/allocate the desired block */
		blocknr_t want_block;
		errorcode_t err = fat_block_map(dir, (current_filename_offset / (blocknr_t)fs->fs_block_size), &want_block, (cur_lfn_chain < 0));
		ANANAS_ERROR_RETURN(err);
		if (want_block != cur_block) {
			bio_free(bio);
			err = vfs_bread(fs, want_block, &bio);
			ANANAS_ERROR_RETURN(err);
			cur_block = want_block;
		}

		/* Fill out the FAT entry */
		uint32_t cur_offs = current_filename_offset % fs->fs_block_size;
		struct FAT_ENTRY* nentry = BIO_DATA(bio) + cur_offs;
		if (cur_entry_idx == chain_needed - 1) {
			/* Final FAT entry; this contains the shortname */
			memcpy(nentry, fentry, sizeof(*nentry));
			/* fsop is the pointer to this entry */
			*(uint64_t*)fsop = cur_block << 16 | cur_offs;
		} else {
			/* LFN entry */
			struct FAT_ENTRY_LFN* lfn = (struct FAT_ENTRY_LFN*)nentry;
			memset(lfn, 0, sizeof(*lfn));
			lfn->lfn_order = cur_entry_idx + 1;
			if (cur_entry_idx + 2 == chain_needed)
				lfn->lfn_order |= LFN_ORDER_LAST;
			lfn->lfn_attribute = FAT_ATTRIBUTE_LFN;
			lfn->lfn_type = 0;
			lfn->lfn_checksum = shortname_checksum;

			/* Mangle the LFN entry in place */
			for (int i = 0; i < 5 && (13 * cur_entry_idx + i) < filename_len; i++)
				lfn->lfn_name_1[i * 2] = dentry[11 * cur_entry_idx + i];
			for (int i = 0; i < 6 && (13 * cur_entry_idx + 5 + i) < filename_len; i++)
				lfn->lfn_name_2[i * 2] = dentry[11 * cur_entry_idx + 5 + i];
			for (int i = 0; i < 2 && (13 * cur_entry_idx + 11 + i) < filename_len; i++)
				lfn->lfn_name_3[i * 2] = dentry[13 * cur_entry_idx + 11 + i];
		}

		bio_set_dirty(bio);
		current_filename_offset += sizeof(struct FAT_ENTRY);
	}

	bio_free(bio);
	return ANANAS_ERROR_OK;
}

static errorcode_t
fat_create(struct VFS_INODE* dir, struct DENTRY_CACHE_ITEM* de, int mode)
{
	struct FAT_ENTRY fentry;
	memset(&fentry, 0, sizeof(fentry));
	fentry.fe_attributes = FAT_ATTRIBUTE_ARCHIVE; /* XXX must be specifiable */

	/* Hook the new file to the directory */
	uint64_t fsop;
	errorcode_t err = fat_add_directory_entry(dir, de->d_entry, &fentry, (void*)&fsop);
	ANANAS_ERROR_RETURN(err);

	/* And obtain it */
	struct VFS_INODE* inode;
	err = vfs_get_inode(dir->i_fs, &fsop, &inode);
	ANANAS_ERROR_RETURN(err);

	/* Almost done - hook it to the dentry */
	dcache_set_inode(de, inode);
	return err;
}

static struct VFS_INODE_OPS fat_dir_ops = {
	.readdir = fat_readdir,
	.lookup = vfs_generic_lookup,
	.create = fat_create
};

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
			inode->i_iops = &fat_file_ops;
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
	inode->i_sb.st_blocks = inode->i_sb.st_size / (fs_privdata->sectors_per_cluster * fs_privdata->sector_size);
}

static errorcode_t
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
		return ANANAS_ERROR_OK;
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
	return ANANAS_ERROR_OK;
}

static errorcode_t
fat_write_inode(struct VFS_INODE* inode)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->fs_privdata;
	struct FAT_INODE_PRIVDATA* privdata = inode->i_privdata;
	uint64_t fsop = *(uint64_t*)inode->i_fsop;
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

	/* And off it goes */
	bio_set_dirty(bio);
	bio_free(bio);
	return ANANAS_ERROR_OK; /* XXX How should we deal with errors? */
}

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
	uint32_t fat_size = FAT_FROM_LE16(bpb->bpb_sectors_per_fat);
	if (fat_size == 0)
		fat_size = FAT_FROM_LE32(bpb->epb.fat32.epb_sectors_per_fat);
	privdata->first_rootdir_sector = privdata->reserved_sectors + (privdata->num_fats * fat_size);
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
	kprintf("fat: FAT%u, fat_size=%u, first_root_sector=%u, first_data_sector=%u\n",
	 privdata->fat_type, fat_size, privdata->first_rootdir_sector, privdata->first_data_sector);
#endif

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

struct VFS_FILESYSTEM_OPS fsops_fat = {
	.mount = fat_mount,
	.alloc_inode = fat_alloc_inode,
	.destroy_inode = fat_destroy_inode,
	.read_inode = fat_read_inode,
	.write_inode = fat_write_inode
};

/* vim:set ts=2 sw=2: */
