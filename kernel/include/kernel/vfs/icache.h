#ifndef __ANANAS_ICACHE_H__
#define __ANANAS_ICACHE_H__

#include <ananas/types.h>
#include "kernel/list.h"

struct VFS_MOUNTED_FS;

void icache_remove_inode(struct VFS_INODE* inode);
/*
 * Removes an inode reference; cleans up the inode if the refcount is zero.
 */
void vfs_deref_inode(struct VFS_INODE* inode);

/*
 * Explicitely add a reference to a given inode.
 */
void vfs_ref_inode(struct VFS_INODE* inode);

/* Low-level interface - designed for filesystem use (must not be used otherwise) */

/* Marks an inode as dirty; will trigger the filesystem's 'write_inode' function */
void vfs_set_inode_dirty(struct VFS_INODE* inode);

/* Internal interface only */
void vfs_dump_inode(struct VFS_INODE* inode);


#endif /* __ANANAS_ICACHE_H__ */
