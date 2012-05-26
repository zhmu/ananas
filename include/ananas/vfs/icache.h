#ifndef __ANANAS_ICACHE_H__
#define __ANANAS_ICACHE_H__

#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/dqueue.h>

struct VFS_MOUNTED_FS;

#define ICACHE_ITEMS_PER_FS	32

struct ICACHE_ITEM {
	struct VFS_INODE* inode;		/* Backing inode, or NULL */
	DQUEUE_FIELDS(struct ICACHE_ITEM);
	char fsop[1];				/* FSOP of the item */
};

DQUEUE_DEFINE(ICACHE_QUEUE, struct ICACHE_ITEM);

void icache_init(struct VFS_MOUNTED_FS* fs);
void icache_dump(struct VFS_MOUNTED_FS* fs);
void icache_destroy(struct VFS_MOUNTED_FS* fs);
void icache_remove_inode(struct VFS_INODE* inode);

#endif /* __ANANAS_ICACHE_H__ */
