#ifndef __FATFS_INODE_H__
#define __FATFS_INODE_H__

#include <ananas/types.h>

class Result;
struct INode;

Result fat_prepare_inode(INode& inode);
void fat_discard_inode(INode& inode);
Result fat_read_inode(INode& inode, ino_t inum);
Result fat_write_inode(INode& inode);

extern struct VFS_INODE_OPS fat_inode_ops;

#endif /* __FATFS_INODE_H__ */
