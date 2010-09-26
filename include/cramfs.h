#include <ananas/types.h>

#ifndef __CRAMFS_H__
#define __CRAMFS_H__

struct CRAMFS_INFO {
	uint32_t	i_crc;
	uint32_t	i_edition;
	uint32_t	i_blocks;
	uint32_t	i_files;
} __attribute__((packed));

struct CRAMFS_INODE {
	uint16_t	in_mode;
	uint16_t	in_uid;
	uint32_t	in_size_gid;				/* 24 bit size, 8 bit gid */
#define CRAMFS_INODE_SIZE(x) (((x)>>8)&0xffffff)
#define CRAMFS_INODE_GID(x)  ((x)&0xff)
	uint32_t	in_namelen_offset;			/* 6 bit namelen, 26 bit offset */
#define CRAMFS_INODE_NAMELEN(x) (((x)>>26)&0x3f)
#define CRAMFS_INODE_OFFSET(x) ((x)&0x7ffffff)
} __attribute__((packed));

struct CRAMFS_SUPERBLOCK {
	uint32_t	c_magic;
#define CRAMFS_MAGIC	0x28cd3d45
	uint32_t	c_size;
	uint32_t	c_flags;
#define CRAMFS_FLAG_FSID2		0x0001			/* fsid version 2 */
#define CRAMFS_FLAG_SORTED_DIRS		0x0002			/* sorted directories */
#define CRAMFS_FLAG_HOLES		0x0100			/* support for holes */
#define CRAMFS_FLAG_SHIFTED_ROOT_OFFSET	0x0400			/* shifted root fs */
	uint32_t	c_reserved;
	uint8_t		c_signature[16];
#define CRAMFS_SIGNATURE	"Compressed ROMFS"
	struct CRAMFS_INFO c_info;
	uint8_t		c_name[16];
	struct CRAMFS_INFO c_rootinode;
} __attribute__((packed));

/* The flags mask that makes the image acceptable */
#define CRAMFS_FLAG_MASK		0x00ff

#endif /* __CRAMFS_H__ */
