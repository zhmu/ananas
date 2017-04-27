#ifndef __ANANAS_DENTRY_H__
#define __ANANAS_DENTRY_H__

#include <ananas/types.h>
#include <ananas/list.h>

struct VFS_MOUNTED_FS;

#define DCACHE_ITEMS_PER_FS	32
#define DCACHE_MAX_NAME_LEN	255

struct VFS_MOUNTED_FS;

struct DENTRY {
	refcount_t d_refcount;			/* Reference count, >0 */
	struct VFS_MOUNTED_FS* d_fs;
	struct DENTRY* d_parent;		/* Parent directory entry */
	struct VFS_INODE* d_inode;		/* Backing entry inode, or NULL */
	uint32_t d_flags;			/* Item flags */
#define DENTRY_FLAG_NEGATIVE	0x0001		/* Negative entry; does not exist */
#define DENTRY_FLAG_PERMANENT	0x0002		/* Entry must not be removed */
#define DENTRY_FLAG_CACHED	0x0004		/* Entry is in the cache */
	char	d_entry[DCACHE_MAX_NAME_LEN];	/* Entry name */
	LIST_FIELDS(struct DENTRY);
};

LIST_DEFINE(DENTRY_QUEUE, struct DENTRY);

struct DENTRY* dcache_create_root_dentry(struct VFS_MOUNTED_FS* fs);

void dcache_dump();
struct DENTRY* dcache_lookup(struct DENTRY* parent, const char* entry);
void dcache_remove_inode(struct VFS_INODE* inode);
void dcache_set_inode(struct DENTRY* de, struct VFS_INODE* inode);

/* Adds a reference to dentry d */
void dentry_ref(struct DENTRY* d);

/* Removes a reference from d; cleans up the item when out of references */
void dentry_deref(struct DENTRY* d);

/* Purges an entry from the cache; this may clean up the item when out of references */
void dcache_purge_entry(struct DENTRY* d);

#endif /*  __ANANAS_DENTRY_H__ */
