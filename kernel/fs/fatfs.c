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
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <fat.h>

#define FAT_FROM_LE16(x) (((uint8_t*)(x))[0] | (((uint8_t*)(x))[1] << 8))
#define FAT_FROM_LE32(x) (((uint8_t*)(x))[0] | (((uint8_t*)(x))[1] << 8) | (((uint8_t*)(x))[2] << 16) | (((uint8_t*)(x))[3] << 24))

struct FAT_FS_PRIVDATA {
	int fat_type;                     /* FAT type: 12, 16 or 32 */
	int	sector_size;                  /* Sector size, in bytes */
	uint32_t sectors_per_cluster;			/* Cluster size (in sectors) */
	uint16_t reserved_sectors;        /* Number of reserved sectors */
	int num_fats;                     /* Number of FATs on disk */
	int num_rootdir_sectors;          /* Number of sectors occupying root dir */
	uint32_t first_rootdir_sector;    /* First sector containing root dir */
	uint32_t first_data_sector;       /* First sector containing file data */
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

/*
 * Reads a given sector.
 */
static struct BIO*
fat_bread(struct VFS_MOUNTED_FS* fs, block_t block)
{
	struct FAT_FS_PRIVDATA* privdata = (struct FAT_FS_PRIVDATA*)fs->privdata;
	return vfs_bread(fs, block, fs->block_size);
}

static block_t
fat_cluster_to_sector(struct VFS_MOUNTED_FS* fs, uint32_t cluster)
{
	struct FAT_FS_PRIVDATA* privdata = (struct FAT_FS_PRIVDATA*)fs->privdata;
	KASSERT(cluster >= 2, "invalid cluster number %u specified", cluster);

	block_t ret = cluster - 2 /* really; the first 2 are reserved or something? */;
	ret *= privdata->sectors_per_cluster;
	ret += privdata->first_data_sector;
	return ret;
}

static struct VFS_INODE*
fat_alloc_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode = vfs_make_inode(fs);
	if (inode == NULL)
		return NULL;
	inode->privdata = kmalloc(sizeof(struct FAT_INODE_PRIVDATA));
	memset(inode->privdata, 0, sizeof(struct FAT_INODE_PRIVDATA));
	return inode;
}

static void
fat_free_inode(struct VFS_INODE* inode)
{
	kfree(inode->privdata);
}

/*
 * Used to obtain the clusternum'th cluster of a file starting at first_cluster. Returns
 * zero if the end marker was detected.
 */
static block_t
fat_get_cluster(struct VFS_MOUNTED_FS* fs, uint32_t first_cluster, uint32_t clusternum)
{
	struct FAT_FS_PRIVDATA* fs_privdata = fs->privdata;
	uint32_t cur_cluster = first_cluster;

	/* XXX this function would benefit a lot from caching */
	while (cur_cluster != 0 && clusternum-- > 0) {
		uint32_t offset;
		switch(fs_privdata->fat_type) {
			case 12:
				offset = cur_cluster + (cur_cluster / 2); /* = 1.5 * cur_cluster */
				break;
			case 16:
				offset = 2 * cur_cluster;
				break;
			case 32:
				offset = 4 * cur_cluster;
				break;
			default:
				panic("fat_get_cluster(): unsupported fat size %u", fs_privdata->fat_type);
		}
		uint32_t sector_num = fs_privdata->reserved_sectors + (offset / fs_privdata->sector_size);
		offset %= fs_privdata->sector_size;
		struct BIO* bio = fat_bread(fs, sector_num);
		/* XXX errors */

		/* Grab the value from the FAT */
		switch (fs_privdata->fat_type) {
			case 12: {
				/*
				 * FAT12 really sucks; a FAT entry may span over the border of the sector.
				 * We'll have to be careful and check for this case...
				 */
				uint16_t fat_value;
				if (offset == (fs_privdata->sector_size - 1)) {
					struct BIO* bio2 = fat_bread(fs, sector_num + 1);
					/* XXX errors */
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
	}
	return cur_cluster;
}

/*
 * Performs a FAT file read; the cur_block/cur_offset parts are needed to construct
 * FSOP's when traversing a directory.
 */
static errorcode_t
fat_do_read(struct VFS_FILE* file, void* buf, size_t* len, block_t* cur_block, int* cur_offset)
{
	struct VFS_INODE* inode = file->inode;
	struct FAT_INODE_PRIVDATA* privdata = inode->privdata;
	struct VFS_MOUNTED_FS* fs = inode->fs;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->privdata;
	size_t written = 0;
	size_t left = *len;

	struct BIO* bio = NULL;
	*cur_block = 0;
	while(left > 0) {
		/*
		 * FAT12/16 root inodes are special as they have a fixed location and
		 * length; the FAT isn't needed to traverse them.
		 */
		block_t want_block;
		if (privdata->root_inode && fs_privdata->fat_type != 32) {
			want_block = fs_privdata->first_rootdir_sector + (file->offset / (block_t)fs->block_size);
			if (file->offset >= (fs_privdata->num_rootdir_sectors * fs_privdata->sector_size))
				break;
		} else {
			uint32_t cluster = fat_get_cluster(fs, privdata->first_cluster, (file->offset / (fs_privdata->sectors_per_cluster * fs_privdata->sector_size)));
			if (cluster == 0) /* end of the chain */
				break;
			want_block  = fat_cluster_to_sector(fs, cluster);
			want_block += ((file->offset % (fs_privdata->sectors_per_cluster * fs_privdata->sector_size)) / fs->block_size);
		}

		/* Grab the block if needed */
		if (*cur_block != want_block) {
			if (bio != NULL) bio_free(bio);
			bio = fat_bread(inode->fs, want_block);
			/* XXX errors */
			*cur_block = want_block;
		}

		/* Walk through the current entry */
		*cur_offset = (file->offset % (block_t)fs->block_size);
		int chunk_len = fs->block_size - *cur_offset;
		if (chunk_len > left)
			chunk_len = left;
		KASSERT(chunk_len > 0, "attempt to handle empty chunk");
		memcpy(buf, (void*)(BIO_DATA(bio) + *cur_offset), chunk_len);

		written += chunk_len;
		buf += chunk_len;
		left -= chunk_len;
		file->offset += chunk_len;
	}
	if (bio != NULL) bio_free(bio);
	*len = written;
	return ANANAS_ERROR_OK;
}

static errorcode_t
fat_read(struct VFS_FILE* file, void* buf, size_t* len)
{
	block_t cur_block;
	int cur_offs;
	return fat_do_read(file, buf, len, &cur_block, &cur_offs);
}

static struct VFS_INODE_OPS fat_file_ops = {
	.read = fat_read,
};

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

/*
 * Used to construct a FAT filename. Returns zero if the filename is not
 * complete.
 */
static int
fat_construct_filename(struct FAT_ENTRY* fentry, char* fat_filename, int* cur_pos)
{
#define ADD_CHAR(c) \
	do { \
			fat_filename[*cur_pos] = (c); \
			(*cur_pos)++; \
	} while(0)

	if (fentry->fe_attributes == FAT_ATTRIBUTE_LFN) {
		struct FAT_ENTRY_LFN* lfnentry = (struct FAT_ENTRY_LFN*)fentry;

		/* LFN XXX should we bother about the checksum? */

		/* LFN, part 1 XXX we throw away the upper 8 bits */
		for (int i = 0; i < 5; i++) {
			ADD_CHAR(lfnentry->lfn_name_1[i * 2]);
		}

		/* LFN, part 2 XXX we throw away the upper 8 bits */
		for (int i = 0; i < 5; i++) {
			ADD_CHAR(lfnentry->lfn_name_2[i * 2]);
		}

		/* LFN, part 3 XXX we throw away the upper 8 bits */
		for (int i = 0; i < 2; i++) {
			ADD_CHAR(lfnentry->lfn_name_3[i * 2]);
		}
		return 0;
	}

	/*
	 * If we have a current LFN entry, we should use that instead and
	 * ignore the old 8.3 filename.
	 */
	if (*cur_pos > 0)
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
		if (ch >= 'A' && ch <= 'Z') /* XXX lowercase */
			ch += 'a' - 'A';
		ADD_CHAR(ch);
	}

#undef ADD_CHAR

	return 1;
}
static errorcode_t
fat_readdir(struct VFS_FILE* file, void* dirents, size_t* len)
{
	struct VFS_INODE* inode = file->inode;
	struct FAT_INODE_PRIVDATA* privdata = inode->privdata;
	struct VFS_MOUNTED_FS* fs = inode->fs;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->privdata;
	size_t written = 0;
	size_t left = *len;
	char cur_filename[128]; /* currently assembled filename */
	int cur_filename_len = 0;
	errorcode_t err = ANANAS_ERROR_OK;

	while(left > 0) {
		struct FAT_ENTRY fentry;
		block_t cur_block;
		int cur_offs;
		size_t entry_length = sizeof(struct FAT_ENTRY);
		err = fat_do_read(file, &fentry, &entry_length, &cur_block, &cur_offs);
		if (err != ANANAS_ERROR_NONE)
			break;
		if (entry_length != sizeof(struct FAT_ENTRY)) {
			err = ANANAS_ERROR(SHORT_READ);
			break;
		}

		if (fentry.fe_filename[0] == '\0') {
			/* First charachter is nul - this means there are no more entries to come */
			break;
		}

		if (fentry.fe_filename[0] == 0xe5) {
			/* Deleted file; we'll skip it */
			continue;
		}

		/* Convert the filename bits */
		if (!fat_construct_filename(&fentry, cur_filename, &cur_filename_len))
			/* This is part of a long file entry name - get more */
			continue;

		/*
		 * If this is a volume entry, ignore it (but do this after the LFN has been handled
		 * because these will always have the volume bit set)
		 */
		if (fentry.fe_attributes & FAT_ATTRIBUTE_VOLUMEID) {
			cur_filename_len = 0;
			continue;
		}

		/* And hand it to the fill function */
		uint64_t fsop = cur_block << 16 | cur_offs;
		int filled = vfs_filldirent(&dirents, &left, (const void*)&fsop, file->inode->fs->fsop_size, cur_filename, cur_filename_len);
		if (!filled) {
			/* out of space! */
			break;
		}
		written += filled; cur_filename_len = 0;
	}
	*len = written;
	return err;
}

static struct VFS_INODE_OPS fat_dir_ops = {
	.readdir = fat_readdir,
	.lookup = vfs_generic_lookup
};

static void
fat_fill_inode(struct VFS_INODE* inode, void* fsop, struct FAT_ENTRY* fentry)
{
	struct VFS_MOUNTED_FS* fs = inode->fs;
	struct FAT_INODE_PRIVDATA* privdata = inode->privdata;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->privdata;

	inode->sb.st_ino = (uint32_t)fsop; /* XXX */
	inode->sb.st_mode = 0755;
	inode->sb.st_nlink = 1;
	inode->sb.st_uid = 0;
	inode->sb.st_gid = 0;
	/* TODO */
	inode->sb.st_atime = 0;
	inode->sb.st_mtime = 0;
	inode->sb.st_ctime = 0;
	if (fentry != NULL) {
		inode->sb.st_size = FAT_FROM_LE32(fentry->fe_size);
		uint32_t cluster  = FAT_FROM_LE16(fentry->fe_cluster_lo);
		if (fs_privdata->fat_type == 32)
			cluster |= FAT_FROM_LE16(fentry->fe_cluster_hi) << 16;
		privdata->first_cluster = cluster;
		/* Distinguish between directory and inode */
		if (fentry->fe_attributes & FAT_ATTRIBUTE_DIRECTORY) {
			inode->iops = &fat_dir_ops;
			inode->sb.st_mode |= S_IFDIR;
		} else {
			inode->iops = &fat_file_ops;
			inode->sb.st_mode |= S_IFREG;
		}
	} else {
		/* Root inode */
		inode->sb.st_size = fs_privdata->num_rootdir_sectors * fs_privdata->sector_size;
		inode->iops = &fat_dir_ops;
		inode->sb.st_mode |= S_IFDIR;
		if (fs_privdata->fat_type == 32)
			privdata->first_cluster = fs_privdata->first_rootdir_sector;
	}
	inode->sb.st_blocks = inode->sb.st_size / (fs_privdata->sectors_per_cluster * fs_privdata->sector_size);
}

static int
fat_read_inode(struct VFS_INODE* inode, void* fsop)
{
	struct VFS_MOUNTED_FS* fs = inode->fs;
	struct FAT_FS_PRIVDATA* fs_privdata = fs->privdata;
	struct FAT_INODE_PRIVDATA* privdata = inode->privdata;

	/*
	 * FAT doesn't really have a root inode, so we just fake one. The root_inode
	 * flag makes the read/write functions do the right thing.
	 */
	if (*(uint64_t*)fsop == FAT_ROOTINODE_FSOP) {
		privdata->root_inode = 1;
		fat_fill_inode(inode, fsop, NULL);
		return 1;
	}

	/*
	 * For ordinary inodes, the fsop is just the block with the 16-bit offset
	 * within that block.
	 */
	uint32_t block  = (*(uint64_t*)fsop) >> 16;
	uint32_t offset = (*(uint64_t*)fsop) & 0xffff;
	KASSERT(offset <= fs->block_size - sizeof(struct FAT_ENTRY), "fsop inode offset %u out of range", offset);
	struct BIO* bio = fat_bread(inode->fs, block);
	/* XXX error handling */
	struct FAT_ENTRY* fentry = (struct FAT_ENTRY*)(void*)(BIO_DATA(bio) + offset);
	/* Fill out the inode details */
	fat_fill_inode(inode, fsop, fentry);
	bio_free(bio);
	return 1;
}

static int
fat_mount(struct VFS_MOUNTED_FS* fs)
{
	/* XXX this should be made a compile-time check */
	KASSERT(sizeof(struct FAT_ENTRY) == 32, "compiler error: fat entry is not 32 bytes!");

	struct BIO* bio = vfs_bread(fs, 0, 512);
	/* XXX handle errors */
	struct FAT_BPB* bpb = (struct FAT_BPB*)BIO_DATA(bio);
	struct FAT_FS_PRIVDATA* privdata = kmalloc(sizeof(struct FAT_FS_PRIVDATA));
	memset(privdata, 0, sizeof(struct FAT_FS_PRIVDATA));
	fs->privdata = privdata; /* immediately, this is used by other functions */

#define FAT_ABORT(x...) \
	do { \
		kfree(privdata); \
		kprintf(x); \
		return 0; \
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
	uint32_t num_data_clusters = (num_sectors - privdata->first_data_sector) / privdata->sectors_per_cluster;
	if (num_data_clusters < 4085) {
		privdata->fat_type = 12;
	} else if (num_data_clusters < 65525) {
		privdata->fat_type = 16;
	} else {
		privdata->fat_type = 32;
		privdata->first_rootdir_sector = FAT_FROM_LE32(bpb->epb.fat32.epb_root_cluster);
	}
	kprintf("fat: FAT%u, fat_size=%u, first_root_sector=%u, first_data_sector=%u\n",
	 privdata->fat_type, fat_size, privdata->first_rootdir_sector, privdata->first_data_sector);

	/* Everything is in order - fill out our filesystem details */
  fs->block_size = privdata->sector_size;
  fs->fsop_size = sizeof(uint64_t);
  fs->root_inode = vfs_alloc_inode(fs);
	uint64_t root_fsop = FAT_ROOTINODE_FSOP;
	if (!fat_read_inode(fs->root_inode, &root_fsop))
		FAT_ABORT("fat: cannot read root inode");
	return 1;
}

struct VFS_FILESYSTEM_OPS fsops_fat = {
	.mount = fat_mount,
	.alloc_inode = fat_alloc_inode,
	.free_inode = fat_free_inode,
	.read_inode = fat_read_inode
};

/* vim:set ts=2 sw=2: */
