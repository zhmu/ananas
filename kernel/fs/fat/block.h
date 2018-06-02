#ifndef __FATFS_BLOCK_H__
#define __FATFS_BLOCK_H__

#include <ananas/types.h>

struct INode;
class Result;

Result fat_block_map(INode& inode, blocknr_t block_in, blocknr_t& block_out, bool create);
void fat_dump_cache(struct VFS_MOUNTED_FS* fs);
int fat_clear_cache(struct VFS_MOUNTED_FS* fs, uint32_t first_cluster);
Result fat_truncate_clusterchain(INode& inode);
Result fat_update_infosector(struct VFS_MOUNTED_FS* fs);

extern struct VFS_INODE_OPS fat_inode_ops;

#endif /* __FATFS_BLOCK_H__ */
