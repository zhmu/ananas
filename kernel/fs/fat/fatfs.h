#ifndef __FATFS_H__
#define __FATFS_H__

#include <ananas/types.h>

/*
 * Used to uniquely identify a FAT16 root inode; it appears on a
 * special, fixed place and has a limited number of entries. FAT32
 * just uses a normal cluster as FAT16 do for subdirectories.
 */
#define FAT_ROOTINODE_FSOP 0xfffffffe

#define FAT_FROM_LE16(x) \
	(((uint8_t*)(x))[0] | (((uint8_t*)(x))[1] << 8))
#define FAT_FROM_LE32(x) \
	(((uint8_t*)(x))[0] | (((uint8_t*)(x))[1] << 8) | (((uint8_t*)(x))[2] << 16) | (((uint8_t*)(x))[3] << 24))

#define FAT_TO_LE16(x, y) \
	do { \
		((uint8_t*)(x))[0] =  (y)       & 0xff; \
		((uint8_t*)(x))[1] = ((y) >> 8) & 0xff; \
	} while(0)

#define FAT_TO_LE32(x, y) \
	do { \
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
	int      fat_type;			/* FAT type: 16 or 32 */
	int	 sector_size;			/* Sector size, in bytes */
	uint32_t sectors_per_cluster;		/* Cluster size (in sectors) */
	uint16_t reserved_sectors;		/* Number of reserved sectors */
	int      num_fats;			/* Number of FATs on disk */
	int      num_rootdir_sectors;		/* Number of sectors occupying root dir */
	uint32_t num_fat_sectors;		/* FAT size in sectors */
	uint32_t first_rootdir_sector;		/* First sector containing root dir */
	uint32_t first_data_sector;		/* First sector containing file data */
	uint32_t total_clusters;		/* Total number of clusters on filesystem */
	uint32_t next_avail_cluster;		/* Next available cluster */
	uint32_t num_avail_clusters;		/* Number of available clusters */
	uint32_t infosector_num;		/* Info sector, or 0 if not present */
	spinlock_t spl_cache;
	struct FAT_CLUSTER_CACHEITEM cluster_cache[FAT_NUM_CACHEITEMS];
};

struct FAT_INODE_PRIVDATA {
	int      root_inode;
	uint32_t first_cluster;
	uint32_t last_cluster;
};

#endif /* __FATFS_H__ */
