#ifndef __FATFS_INODE_H__
#define __FATFS_INODE_H__

#include <ananas/types.h>

struct VFS_INODE;
struct DENTRY_CACHE_ITEM;

struct VFS_INODE* fat_alloc_inode(struct VFS_MOUNTED_FS* fs, const void* fsop);
void fat_destroy_inode(struct VFS_INODE* inode);
errorcode_t fat_read_inode(struct VFS_INODE* inode, void* fsop);
errorcode_t fat_write_inode(struct VFS_INODE* inode);

extern struct VFS_INODE_OPS fat_inode_ops;

#endif /* __FATFS_INODE_H__ */
