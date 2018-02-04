#ifndef __FATFS_DIR_H__
#define __FATFS_DIR_H__

#include <ananas/types.h>

struct VFS_FILE;

Result fat_readdir(struct VFS_FILE* file, void* dirents, size_t* len);
extern struct VFS_INODE_OPS fat_dir_ops;

#endif /* __FATFS_DIR_H__ */
