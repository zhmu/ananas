#ifndef __ANANAS_DENTRY_H__
#define __ANANAS_DENTRY_H__

#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/dqueue.h>

struct VFS_MOUNTED_FS;

#define DCACHE_ITEMS_PER_FS	20
#define DCACHE_MAX_NAME_LEN	255

struct DENTRY_CACHE_ITEM {
	struct VFS_INODE* d_dir_inode;		/* Directory inode */
	struct VFS_INODE* d_entry_inode;	/* Backing entry inode, or NULL */
	uint32_t d_flags;			/* Item flags */
#define DENTRY_FLAG_NEGATIVE	0x0001		/* Negative entry; does not exist */
#define DENTRY_FLAG_PERMANENT	0x0002		/* Entry must not be removed */
	char	d_entry[DCACHE_MAX_NAME_LEN];	/* Entry name */
	DQUEUE_FIELDS(struct DENTRY_CACHE_ITEM);
};

DQUEUE_DEFINE(DENTRY_CACHE_QUEUE, struct DENTRY_CACHE_ITEM);

void dcache_init(struct VFS_MOUNTED_FS* fs);
void dcache_dump(struct VFS_MOUNTED_FS* fs);
void dcache_destroy(struct VFS_MOUNTED_FS* fs);
struct DENTRY_CACHE_ITEM* dcache_find_item_or_add_pending(struct VFS_INODE* inode, const char* entry);
void dcache_remove_inode(struct VFS_INODE* inode);
void dcache_set_inode(struct DENTRY_CACHE_ITEM* de, struct VFS_INODE* inode);

#endif /*  __ANANAS_DENTRY_H__ */
