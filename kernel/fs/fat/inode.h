#ifndef __FATFS_INODE_H__
#define __FATFS_INODE_H__

#include <ananas/types.h>

struct INode;

errorcode_t fat_prepare_inode(INode& inode);
void fat_discard_inode(INode& inode);
errorcode_t fat_read_inode(INode& inode, ino_t inum);
errorcode_t fat_write_inode(INode& inode);

extern struct VFS_INODE_OPS fat_inode_ops;

#endif /* __FATFS_INODE_H__ */
