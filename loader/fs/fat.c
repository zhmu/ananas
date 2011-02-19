/*
 * This is our FAT driver; it supports FAT12/16/32 including long file names.
 */
#include <sys/types.h>
#include <loader/diskio.h>
#include <loader/vfs.h>
#include <stdio.h>
#include <string.h>
#include <fat.h>

#ifdef FAT

#undef DEBUG_FAT

#define FAT_MAX_FILENAME_LEN 256

struct FAT_MOUNTED_FILESYSTEM {
	/* device we are doing I/O with */
	int      fat_device;
	/* fat type: 12, 16 or 32 */
	int      fat_type;
	/* sector size, only 512 is accepted for now */
	uint32_t fat_sector_size;
	/* sectors per cluster, must be a power of 2 */
	uint32_t fat_sectors_per_cluster;
	/* number of reserved sectors */
	uint32_t fat_reserved_sectors;
	/*
	 * fat12/16: first sector detailing the root directory
	 * fat32: first cluster detailing the root directory
	 */
  uint32_t fat_first_rootdir_item;
	/* fat12/16: number of sectors occupied by the root directory */
	uint32_t fat_num_rootdir_sectors;
	/* first sector containing actual data */
	uint32_t fat_first_data_sector;
	/* constructed filename */
	char     fat_filename[FAT_MAX_FILENAME_LEN];
	/* next file size (filled by readdir, used by open) */
	uint32_t fat_next_size;
	/* next file cluster (filled by readdir, used by open) */
	uint32_t fat_next_cluster;
};

struct FAT_ACTIVE_FILE {
	/* fat12/16: non-zero if this is the root directory */
	int      file_isroot;
	/* first data cluster */
	uint32_t file_first_cluster;
};

static struct FAT_MOUNTED_FILESYSTEM* fat_fsinfo;
static struct FAT_ACTIVE_FILE* fat_activefile;

#define FAT_FROM_LE16(x) (((uint8_t*)(x))[0] | (((uint8_t*)(x))[1] << 8))
#define FAT_FROM_LE32(x) (((uint8_t*)(x))[0] | (((uint8_t*)(x))[1] << 8) | (((uint8_t*)(x))[2] << 16) | (((uint8_t*)(x))[3] << 24))

static inline uint32_t
fat_get_cluster(uint32_t first_cluster, uint32_t cluster_num)
{
	uint32_t cur_cluster = first_cluster;

	/* XXX This is a mess; we should just cache the cluster numbers */
	while (cur_cluster != 0 && cluster_num-- > 0) {
		uint32_t offset = cur_cluster;
		switch(fat_fsinfo->fat_type) {
			case 12:
				offset += cur_cluster / 2; /* offset *= 1.5; */
				break;
			case 16:
				offset *= 2;
				break;
			case 32:
				offset *= 4;
				break;
		}

		uint32_t sector_num = fat_fsinfo->fat_reserved_sectors + (offset / fat_fsinfo->fat_sector_size);
		offset %= fat_fsinfo->fat_sector_size;
		struct CACHE_ENTRY* centry = diskio_read(fat_fsinfo->fat_device, sector_num);
		if (centry == NULL)
			return 0;

		switch(fat_fsinfo->fat_type) {
			case 12: {
				/* FAT12 sucks: the the block can span over 2 sectors */
				uint16_t fat_value;
				if (offset < (fat_fsinfo->fat_sector_size - 1)) {
					fat_value = FAT_FROM_LE16((char*)(centry->data + offset));
				} else {
					/* Seems we hit the split-sector case; read an extra one and merge them */
					struct CACHE_ENTRY* centry2 = diskio_read(fat_fsinfo->fat_device, sector_num + 1);
					if (centry2 == NULL)
						return 0;
					fat_value = centry ->data[offset];
					fat_value |= centry2->data[0] << 8;
				}
				if (cur_cluster & 1) {
					fat_value >>= 4;
				} else {
					fat_value &= 0xfff;
				}
				cur_cluster = fat_value;
				if (cur_cluster >= 0xff8)
					return 0;
				break;
			}
			case 16:
				cur_cluster = FAT_FROM_LE16((char*)(centry->data + offset));
				if (cur_cluster >= 0xfff8)
					cur_cluster = 0;
				break;
			case 32:
				cur_cluster = FAT_FROM_LE32((char*)(centry->data + offset)) & 0xfffffff;
				if (cur_cluster >= 0xffffff8)
					cur_cluster = 0;
				break;
		}
	}

	return cur_cluster;
}

static inline uint32_t
fat_cluster_to_block(uint32_t cluster)
{
	uint32_t block = cluster - 2; /* the first 2 clusters are bogus */
	block *= fat_fsinfo->fat_sectors_per_cluster;
	block += fat_fsinfo->fat_first_data_sector;
	return block;
}

static size_t
fat_read(void* buf, size_t len)
{
	struct CACHE_ENTRY* centry;
	size_t num_read = 0;

	while (len > 0) {
		/* Figure out which block we need */
		uint32_t want_block;
		if (fat_activefile->file_isroot) {
			/* FAT12/16 root directory; this resides on a fixed location */
			want_block = fat_fsinfo->fat_first_rootdir_item + (vfs_curfile_offset / fat_fsinfo->fat_sector_size);
			if (vfs_curfile_offset >= fat_fsinfo->fat_num_rootdir_sectors * fat_fsinfo->fat_sector_size) {
				/* End of root directory reached */
				return 0;
			}
		} else {
			/* Either not FAT12/16 or not root; need to traverse to the correct cluster */
			uint32_t want_cluster = fat_get_cluster(fat_activefile->file_first_cluster, vfs_curfile_offset / (fat_fsinfo->fat_sector_size * fat_fsinfo->fat_sectors_per_cluster));
			want_block  = fat_cluster_to_block(want_cluster);
			want_block += (vfs_curfile_offset % (fat_fsinfo->fat_sector_size * fat_fsinfo->fat_sectors_per_cluster)) / fat_fsinfo->fat_sector_size;
#ifdef DEBUG_FAT
			printf("fat_read(): offset=%u, first_cluster=%u => want_cluster=%u, want_block=%u\n",
			 vfs_curfile_offset, fat_activefile->file_first_cluster, want_cluster, want_block);
#endif 
		}

		/* Fetch the block; the disk I/O layer will deal with freeing old ones etc */ 
		centry = diskio_read(fat_fsinfo->fat_device, want_block);
		if (centry == NULL)
			return 0;

		int chunklen = SECTOR_SIZE - (vfs_curfile_offset % SECTOR_SIZE);
		if (chunklen > len)
			chunklen = len;
		memcpy(buf, (void*)(centry->data + (vfs_curfile_offset % SECTOR_SIZE)), chunklen);
		len -= chunklen; buf += chunklen;
		num_read += chunklen; vfs_curfile_offset += chunklen;
	}

	return num_read;
}

static int
fat_mount(int device)
{
	struct CACHE_ENTRY* centry;

	/* Read the initial sector; this contains vital information */
	centry = diskio_read(device, 0);
	if (centry == NULL)
		return 0;

	/* Initialize our scratchpad memory */
	struct FAT_BPB* bpb = (struct FAT_BPB*)centry->data;
	fat_fsinfo = vfs_scratchpad;
	memset(fat_fsinfo, 0, sizeof *fat_fsinfo);
	fat_fsinfo->fat_device = device;
	fat_activefile = (struct FAT_ACTIVE_FILE*)(vfs_scratchpad + sizeof *fat_fsinfo);
	memset(fat_activefile, 0, sizeof *fat_activefile);

	/* Fill out our fsinfo; we'll use this to see if the filesystem checks out */
	fat_fsinfo->fat_sector_size = FAT_FROM_LE16(bpb->bpb_bytespersector);
	if (fat_fsinfo->fat_sector_size != 512)
		/* Anything but 512 byte sectors is either very old or very new - reject for now */
		return 0;
	fat_fsinfo->fat_reserved_sectors = FAT_FROM_LE16(bpb->bpb_num_reserved);
	fat_fsinfo->fat_sectors_per_cluster = bpb->bpb_sectorspercluster;
	/*
	 * Checking the clustersize for a power of two may be overly paranoid, but
 	 * we'll quite sure it'll be FAT if this works out
	 */
  int log2_cluster_size = 0;
  for (int i = fat_fsinfo->fat_sectors_per_cluster; i > 1; i >>= 1, log2_cluster_size++)
    ;
  if ((1 << log2_cluster_size) != fat_fsinfo->fat_sectors_per_cluster)
		return 0;
	int num_fats = bpb->bpb_num_fats;
	if (num_fats != 1 && num_fats != 2)
		/* There must be one or two FAT's; we give up otherwise */
		return 0;
	uint32_t fat_size = FAT_FROM_LE16(bpb->bpb_sectors_per_fat);
	if (fat_size == 0)
		fat_size = FAT_FROM_LE32(bpb->epb.fat32.epb_sectors_per_fat);
	int first_rootdir_sector = fat_fsinfo->fat_reserved_sectors + (num_fats * fat_size);
	fat_fsinfo->fat_num_rootdir_sectors = (sizeof(struct FAT_ENTRY) * FAT_FROM_LE16(bpb->bpb_num_root_entries) + fat_fsinfo->fat_sector_size - 1) / fat_fsinfo->fat_sector_size;
	fat_fsinfo->fat_first_data_sector = first_rootdir_sector + fat_fsinfo->fat_num_rootdir_sectors;
	uint32_t num_sectors = FAT_FROM_LE16(bpb->bpb_num_sectors);
	if (num_sectors == 0)
		num_sectors = FAT_FROM_LE32(bpb->bpb_large_num_sectors);
	if (num_sectors == 0)
		return 0;
	int num_data_clusters = (num_sectors - fat_fsinfo->fat_first_data_sector) / fat_fsinfo->fat_sectors_per_cluster;
	if (num_data_clusters < 65525) {
		fat_fsinfo->fat_type = (num_data_clusters < 4085) ? 12 : 16;
		fat_fsinfo->fat_first_rootdir_item = first_rootdir_sector;
		fat_activefile->file_isroot = 1;
		fat_activefile->file_first_cluster = 0;
	} else {
		fat_fsinfo->fat_type = 32;
		fat_fsinfo->fat_first_rootdir_item = FAT_FROM_LE32(bpb->epb.fat32.epb_root_cluster);
		fat_activefile->file_first_cluster = fat_fsinfo->fat_first_rootdir_item;
	}

#ifdef DEBUG_FAT
		printf("fat: fat%u, sector size=%u, sectors per cluster=%u, first data sector=%u\n",
		 fat_fsinfo->fat_type, fat_fsinfo->fat_sector_size, fat_fsinfo->fat_sectors_per_cluster,
		 fat_fsinfo->fat_first_data_sector);
#endif

	return 1;
}

static inline int
fat_construct_filename(struct FAT_ENTRY* fentry, char* fat_filename)
{
#define ADD_CHAR(c) \
	do { \
			fat_filename[pos] = (c); \
			pos++; \
	} while(0)

	int pos = 0;
	if (fentry->fe_attributes == FAT_ATTRIBUTE_LFN) {
		struct FAT_ENTRY_LFN* lfnentry = (struct FAT_ENTRY_LFN*)fentry;
		/* LFN entries needn't be sequential, so fill them out as they pass by */
		pos = ((lfnentry->lfn_order & ~LFN_ORDER_LAST) - 1) * 13;

		/*
		 * Hook the LFN bits together XXX We ignore the checksum and
		 * any UTF-16 stuff (we just use the low byte)
	 	 */
		for (int i = 0; i < 5; i++)
			ADD_CHAR(lfnentry->lfn_name_1[i * 2]);
		for (int i = 0; i < 6; i++)
			ADD_CHAR(lfnentry->lfn_name_2[i * 2]);
		for (int i = 0; i < 2; i++)
			ADD_CHAR(lfnentry->lfn_name_3[i * 2]);
		return 0;
	}

	/*
	 * We've hit the plain 8.3 filename now; if we managed to construct any
	 * LFN entry, we should use that instead and ignore the 8.3 entry.
	 */
	if (fat_filename[0] != '\0')
		return 1;

	/* Convert the 8.3 filename bits */
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

static const char*
fat_readdir()
{
	struct FAT_ENTRY fentry;

	/* Reset the filename construction */
	memset(fat_fsinfo->fat_filename, 0, FAT_MAX_FILENAME_LEN);

	while (1) {
		size_t num_read = fat_read(&fentry, sizeof(fentry));
		if (num_read != sizeof(fentry))
			return NULL;

		/* Bail if the first character is nul, no more will follow */
		if (fentry.fe_filename[0] == '\0')
			break;

		/* Skip deleted files */
		if (fentry.fe_filename[0] == 0xe5)
			continue;

		/* Construct the filename; continues if it's not yet complete */
		if (!fat_construct_filename(&fentry, fat_fsinfo->fat_filename))
			continue;

		/*
		 * Skip volume labels; this must be done after LFN as LFN entries use the
		 * volume bit to identify themselves.
		 */
		if (fentry.fe_attributes & FAT_ATTRIBUTE_VOLUMEID)
			continue;

		/* Set up the next size/cluster for open() */
		fat_fsinfo->fat_next_size = FAT_FROM_LE32(fentry.fe_size);
    uint32_t cluster = FAT_FROM_LE16(fentry.fe_cluster_lo);
    if (fat_fsinfo->fat_type == 32)
      cluster |= FAT_FROM_LE16(fentry.fe_cluster_hi) << 16;
    fat_fsinfo->fat_next_cluster = cluster;

		return fat_fsinfo->fat_filename;
	}
	return NULL;
}

static int
fat_open(const char* fname)
{
	/* Start at the root directory */
	fat_activefile->file_isroot = (fat_fsinfo->fat_type != 32);
	fat_activefile->file_first_cluster = fat_fsinfo->fat_first_rootdir_item;

	/* And wade through the paths */
	while (1) {
		vfs_curfile_offset = 0;

		/* Throw away trailing slashes */
		while (*fname == '/')
			fname++;
		if (*fname == '\0') {
			/* All done */
			return 1;
		}

		/* Figure out how large this part is */
		char* ptr = strchr(fname, '/');
		if (ptr == NULL)
			ptr = strchr(fname, '\0');
		int len = ptr - fname;

		/* Walk through the entries */
		int found = 0;
		const char* entry;
		while ((entry = fat_readdir()) != NULL) {
			if (strncmp(entry, fname, len) == 0) {
				/* Got it */
				found++; fname += len;

				/*
				 * Activate the new file - we use the fact that the start of the
				 * fat_open() loop resets the current file offset.
				 */
				fat_activefile->file_isroot = 0;
				fat_activefile->file_first_cluster = fat_fsinfo->fat_next_cluster;
				vfs_curfile_length = fat_fsinfo->fat_next_size;
				break;
			}
		}

		if (!found) {
			/* This part is not present - bail */
			return 0;
		}
	}

	/* NOTREACHED */
}

struct LOADER_FS_DRIVER loaderfs_fat = {
	.mount = fat_mount,
	.open = fat_open,
	.read = fat_read,
	.readdir = fat_readdir,
};

#endif /* FAT */

/* vim:set ts=2 sw=2: */
