/*
 * This is a readonly ext2fs driver for the loader; as the loader can only read
 * single sectors, it greatly abuses the fact that a block size is always a
 * multiple of 1024.
 */
#include <sys/types.h>
#include <loader/diskio.h>
#include <loader/vfs.h>
#include <stdio.h>
#include <string.h>
#include <ext2.h>

#ifdef EXT2

struct EXT2_MOUNTED_FILESYSTEM {
	int      device;
	uint32_t inode_count;
	uint32_t inode_size;
	uint32_t inodes_per_group;
	uint32_t block_count;
	uint32_t block_size;
	uint32_t blocks_per_group;
	uint32_t first_data_block;
	uint32_t num_blockgroups;
};

struct EXT2_ACTIVE_INODE {
	uint32_t block[EXT2_INODE_BLOCKS];
	char     cur_entry[256];
	uint32_t cur_inode;
};

static struct EXT2_MOUNTED_FILESYSTEM* ext2_fsinfo;
static struct EXT2_ACTIVE_INODE* ext2_activeinode;

#ifdef _BIG_ENDIAN
#define EXT2_TO_LE32(x) ((x >> 16) | (((x & 0xffff)) << 16))
#define EXT2_TO_LE16(x) ((x >> 8) | ((x & 0xff) << 8))
#else
#define EXT2_TO_LE32(x) (x)
#define EXT2_TO_LE16(x) (x)
#endif

/* Locates the block-th block of the active inode */
static uint32_t
ext2_find_block(uint32_t block)
{
	uint32_t pointers_per_block = (ext2_fsinfo->block_size / sizeof(uint32_t));
	uint32_t sectors_per_block = (ext2_fsinfo->block_size / SECTOR_SIZE);

	/* direct blocks */
	if (block < 12)
		return ext2_activeinode->block[block];

	/* first indirect block */
	if (block < 12 + pointers_per_block) {
		uint32_t blknum = ext2_activeinode->block[12];
		blknum *= sectors_per_block;
		struct CACHE_ENTRY* centry = diskio_read(
		 ext2_fsinfo->device,
		 blknum + (((block - 12) * sizeof(uint32_t)) / SECTOR_SIZE)
		);
		return EXT2_TO_LE32(*(uint32_t*)(centry->data + ((block - 12) * sizeof(uint32_t)) % SECTOR_SIZE));
	}

	/* double indirect block */
	if (block < (12 + pointers_per_block + (pointers_per_block * pointers_per_block))) {
		/* get the doubly-indirect block map */
		uint32_t indirect_blknum = (block - 12 - pointers_per_block);
		uint32_t first_block = ext2_activeinode->block[13] * sectors_per_block;
		struct CACHE_ENTRY* centry1 = diskio_read(
		 ext2_fsinfo->device,
		 first_block + ((indirect_blknum / pointers_per_block) * sizeof(uint32_t) / SECTOR_SIZE)
		);
		/* now read the first indirect inside that map */
		uint32_t blknum = EXT2_TO_LE32(*(uint32_t*)(centry1->data + (((indirect_blknum / pointers_per_block) * sizeof(uint32_t)) % SECTOR_SIZE)));
		//printf("double indirect for block=%u:=ind blkum=%u, block=%u -> ", block, indirect_blknum, blknum);
		blknum *= sectors_per_block;
		struct CACHE_ENTRY* centry2 = diskio_read(ext2_fsinfo->device, blknum + (((indirect_blknum % pointers_per_block) * sizeof(uint32_t)) / SECTOR_SIZE));
		uint32_t r = EXT2_TO_LE32(*(uint32_t*)(centry2->data + (((indirect_blknum % pointers_per_block) * sizeof(uint32_t)) % SECTOR_SIZE)));
		//printf("result=%u\n", r);
		return r;
	}

	printf("ext2_find_block() needs support for triple indirect blocks!\n");
	return 0;
}

/* Reads data from the currently active inode */
static size_t
ext2_read(void* buf, size_t len)
{
	/* Normalize len so that it cannot expand beyond the file size */
	if (vfs_curfile_offset + len > vfs_curfile_length)
		len = vfs_curfile_length - vfs_curfile_offset;

	size_t numread = 0;
	while(len > 0) {
		uint32_t blocknum = vfs_curfile_offset / ext2_fsinfo->block_size;
		uint32_t offset = vfs_curfile_offset % ext2_fsinfo->block_size;
		uint32_t block = ext2_find_block(blocknum);
		if (block == 0) {
			/* We've run out of blocks; this shouldn't happen... */
			printf("ext2_read(): no block found, maybe sparse file?\n");
			break;
		}

		int chunklen = SECTOR_SIZE - (offset % SECTOR_SIZE);
		if (chunklen > len)
			chunklen = len;
		block *= (ext2_fsinfo->block_size / SECTOR_SIZE);
		struct CACHE_ENTRY* centry = diskio_read(
		 ext2_fsinfo->device,
		 block + (offset / SECTOR_SIZE));
		memcpy(buf, (void*)(centry->data + (offset % SECTOR_SIZE)), chunklen);
		len -= chunklen; buf += chunklen;	

		/*
		 * Calculate the chunk length and update the pointers.
		 */
		vfs_curfile_offset += chunklen;
		numread += chunklen;
	}

	return numread;
}

static const char*
ext2_readdir()
{
	/*
	 * Directories are just an inode with specific data (but it uses variable
   * lengths! Thus, just store the current offset and read the largest entry
	 * possible; we'll adjust the offset to point to the next entry.
	 */
	char buf[SECTOR_SIZE];
	uint32_t new_offset = vfs_curfile_offset;
	int len = ext2_read(buf, sizeof(buf));
	if (len == 0)
		return NULL;

	uint32_t buf_offs = 0;
	while (buf_offs < len) {
		struct EXT2_DIRENTRY* ext2de = (struct EXT2_DIRENTRY*)(void*)(buf + buf_offs);
		new_offset += EXT2_TO_LE16(ext2de->rec_len);
		buf_offs +=  EXT2_TO_LE16(ext2de->rec_len);
		uint32_t inum = EXT2_TO_LE32(ext2de->inode);
		if (inum > 0) {
			/*
			 * Inode number values of zero indicate the entry is not used; this entry
			 * works and we must return it.
			 */
			memcpy(ext2_activeinode->cur_entry, ext2de->name, ext2de->name_len);
			ext2_activeinode->cur_entry[ext2de->name_len] = '\0';
			vfs_curfile_offset = new_offset;
			ext2_activeinode->cur_inode = inum;
			return ext2_activeinode->cur_entry;
		}
	}
	ext2_activeinode->cur_inode = 0;
	return NULL;
}

static void	
ext2_dump_inode(struct EXT2_INODE* inode)
{
	printf("mode    = 0x%x\n", inode->i_mode);
	printf("uid/gid = %u:%u\n", inode->i_uid, inode->i_gid);
	printf("size    = %u\n", inode->i_size);
	printf("blocks  =");
	for(int n = 0; n < 15; n++) {
		printf(" %u", inode->i_block[n]);
	}
	printf(" \n");
}

/*
 * Reads a filesystem inode and fills a corresponding inode structure.
 */
static void
ext2_read_inode(uint32_t inum)
{
	inum--;

	/*
	 * Every block group has a fixed number of inodes, so we can find the
	 * blockgroup number and corresponding inode index within this blockgroup by
	 * simple divide and modulo operations. These two are combined to figure out
	 * the block we have to read.
	 */
	uint32_t bgroup = inum / ext2_fsinfo->inodes_per_group;
	uint32_t iindex = inum % ext2_fsinfo->inodes_per_group;

	/*
	 * Figure out the correct block group we have to fetch, and grab the
	 * one and only block our data lives in.
	 */
	uint32_t blocknum = 1 + (bgroup * sizeof(struct EXT2_BLOCKGROUP)) / ext2_fsinfo->block_size;
	blocknum += ext2_fsinfo->first_data_block;
	blocknum *= (ext2_fsinfo->block_size / SECTOR_SIZE);
	blocknum += ((bgroup * sizeof(struct EXT2_BLOCKGROUP)) % ext2_fsinfo->block_size) / SECTOR_SIZE;
	struct CACHE_ENTRY* centry = diskio_read(
	 ext2_fsinfo->device,
	 blocknum);

	struct EXT2_BLOCKGROUP* bg = (struct EXT2_BLOCKGROUP*) (
		centry->data + ((bgroup * sizeof(struct EXT2_BLOCKGROUP)) % SECTOR_SIZE)
	);

	blocknum  =  bg->bg_inode_table + (iindex * ext2_fsinfo->inode_size) / ext2_fsinfo->block_size;
	blocknum *= (ext2_fsinfo->block_size / SECTOR_SIZE);
	blocknum += ((iindex * ext2_fsinfo->inode_size) % ext2_fsinfo->block_size) / SECTOR_SIZE;
	
	/* Fetch the block and make a pointer to the inode */
	centry = diskio_read(ext2_fsinfo->device,
		blocknum
	);
	unsigned int idx = (iindex * ext2_fsinfo->inode_size) % SECTOR_SIZE;
	struct EXT2_INODE* ext2inode = (struct EXT2_INODE*)((void*)centry->data + idx);

	/*
	 * Copy necessary information over to our currently active file structure.
	 */
	vfs_curfile_offset = 0;
	vfs_curfile_length = EXT2_TO_LE32(ext2inode->i_size);
	for (unsigned int i = 0; i < EXT2_INODE_BLOCKS; i++)
		ext2_activeinode->block[i] = EXT2_TO_LE32(ext2inode->i_block[i]);
}

static int
ext2_open(const char* fname)
{
	uint32_t cur_dir_inode = EXT2_ROOT_INO;
	while (1) {
		ext2_read_inode(cur_dir_inode);

		while (*fname == '/')
			fname++;
		if (*fname == '\0')
			/* All done! */
			return 1;

		/* Figure out how much bytes we have to check */
		char* ptr = strchr(fname, '/');
		if (ptr == NULL)
			ptr = strchr(fname, '\0');
		int len = ptr - fname;

		/* Walk through the directory */
		cur_dir_inode = 0;
		const char* entry;
		while ((entry = ext2_readdir()) != NULL) {
			if (strncmp(entry, fname, len) == 0) {
				cur_dir_inode = ext2_activeinode->cur_inode;
				fname += len;
				break;
			}
		}
		if (!cur_dir_inode)
			return 0;
	}
}

static int
ext2_mount(int device)
{
	struct EXT2_SUPERBLOCK sb;
	struct CACHE_ENTRY* centry;

	/* Read the superblock; this has to be done in two parts */
	centry = diskio_read(device, 2);
	if (centry == NULL)
		return 0;
	memcpy((char*)(&sb), centry->data, SECTOR_SIZE);
	centry = diskio_read(device, 3);
	if (centry == NULL)
		return 0;
	memcpy((char*)(&sb + SECTOR_SIZE), centry->data, SECTOR_SIZE);
	if (sb.s_magic != EXT2_SUPER_MAGIC) {
		/* This is not an ext2 file system */
		return 0;
	}

	/* Initialize our scratchpad memory */
	ext2_fsinfo = vfs_scratchpad;
	ext2_activeinode = (void*)((uint32_t)vfs_scratchpad + sizeof(struct EXT2_MOUNTED_FILESYSTEM));
	ext2_fsinfo->device = device;

	/* Copy or calculate all information we need */
	ext2_fsinfo->inode_count = sb.s_inodes_count;
	ext2_fsinfo->inode_size = sb.s_inode_size;
	ext2_fsinfo->inodes_per_group = sb.s_inodes_per_group;
	ext2_fsinfo->block_count = sb.s_blocks_count;
	ext2_fsinfo->blocks_per_group = sb.s_blocks_per_group;
	ext2_fsinfo->first_data_block = sb.s_first_data_block;
	ext2_fsinfo->block_size = 1024L << sb.s_log_block_size;
	ext2_fsinfo->num_blockgroups = (ext2_fsinfo->block_count - ext2_fsinfo->first_data_block - 1) / ext2_fsinfo->blocks_per_group + 1;
	return 1;
}

struct LOADER_FS_DRIVER loaderfs_ext2 = {
	.mount = ext2_mount,
	.open = ext2_open,
	.read = ext2_read,
	.readdir = ext2_readdir,
};

#endif /* EXT2 */

/* vim:set ts=2 sw=2: */
