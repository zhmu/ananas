#ifndef __ANANAS_ICACHE_H__
#define __ANANAS_ICACHE_H__

#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/list.h>

struct VFS_MOUNTED_FS;

#define ICACHE_ITEMS_PER_FS	32

struct ICACHE_ITEM {
	struct VFS_INODE* inode;		/* Backing inode, or NULL */
	LIST_FIELDS(struct ICACHE_ITEM);
	char fsop[1];				/* FSOP of the item */
};

LIST_DEFINE(ICACHE_QUEUE, struct ICACHE_ITEM);

void icache_init(struct VFS_MOUNTED_FS* fs);
void icache_dump(struct VFS_MOUNTED_FS* fs);
void icache_destroy(struct VFS_MOUNTED_FS* fs);
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

/*
 * Creates a new, empty inode for the given filesystem. The corresponding inode is
 * locked, has a refcount of one and must be filled by the filesystem before
 * unlocking.
 */
struct VFS_INODE* vfs_make_inode(struct VFS_MOUNTED_FS* fs, const void* fsop);

/*
 * Destroys a locked inode; this should be called by the filesystem's
 * 'destroy_inode' function.
 */
void vfs_destroy_inode(struct VFS_INODE* inode);

/* Marks an inode as dirty; will trigger the filesystem's 'write_inode' function */
void vfs_set_inode_dirty(struct VFS_INODE* inode);

/* Internal interface only */
void vfs_dump_inode(struct VFS_INODE* inode);


#endif /* __ANANAS_ICACHE_H__ */
