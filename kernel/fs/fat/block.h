#ifndef __FATFS_BLOCK_H__
#define __FATFS_BLOCK_H__

#include <ananas/types.h>

struct VFS_INODE;

errorcode_t fat_block_map(struct VFS_INODE* inode, blocknr_t block_in, blocknr_t* block_out, int create);
void fat_dump_cache(struct VFS_MOUNTED_FS* fs);
int fat_clear_cache(struct VFS_MOUNTED_FS* fs, uint32_t first_cluster);

extern struct VFS_INODE_OPS fat_inode_ops;

#endif /* __FATFS_BLOCK_H__ */
