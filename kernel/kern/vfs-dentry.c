/*
 * Ananas dentry cache (heavily based on vfs-icache.c)
 *
 * A 'dentry' is a directory entry, and can be seen as the function f:
 * directory_inode x entry_name -> inode. These are cached on a per-filesystem
 * basis (to simplify the implementation due to the variable-length FSOP's)
 *
 * Note that this code depends heavily on the fact that an inode will never be
 * in memory multiple times; this implies that the inode pointer can be used
 * to unique identify a given inode.
 * 
 */
#include <ananas/types.h>
#include <ananas/vfs.h>
#include <ananas/dentry.h>
#include <ananas/mm.h>
#include <ananas/lock.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/lib.h>

TRACE_SETUP;

void
dcache_init(struct VFS_MOUNTED_FS* fs)
{
	spinlock_init(&fs->spl_dcache);
	DQUEUE_INIT(&fs->dcache_inuse);
	DQUEUE_INIT(&fs->dcache_free);

	/*
	 * Make an empty cache; we allocate one big pool and set up pointers to the
	 * items as necessary.
	 */
	fs->dcache_buffer = kmalloc(DCACHE_ITEMS_PER_FS * (sizeof(struct DENTRY_CACHE_ITEM) + fs->fsop_size));
	memset(fs->dcache_buffer, 0, DCACHE_ITEMS_PER_FS * (sizeof(struct DENTRY_CACHE_ITEM) + fs->fsop_size));
	addr_t dcache_ptr = (addr_t)fs->dcache_buffer;
	for (int i = 0; i < DCACHE_ITEMS_PER_FS; i++) {
		DQUEUE_ADD_TAIL(&fs->dcache_free, (struct DENTRY_CACHE_ITEM*)dcache_ptr);
		dcache_ptr += sizeof(struct DENTRY_CACHE_ITEM) + fs->fsop_size;
	}
}

void
dcache_dump(struct VFS_MOUNTED_FS* fs)
{
	spinlock_lock(&fs->spl_dcache);
	kprintf("dcache_dump(): fs=%p\n", fs);
	DQUEUE_FOREACH(&fs->dcache_inuse, d, struct DENTRY_CACHE_ITEM) {
		kprintf("dcache_entry=%p, dir_inode=%p, entry_inode=%p, name='%s', flags=0x%x\n",
		 d, d->d_dir_inode, d->d_entry_inode, d->d_entry, d->d_flags);
	}		
	spinlock_unlock(&fs->spl_dcache);
}

void
dcache_destroy(struct VFS_MOUNTED_FS* fs)
{	
	spinlock_lock(&fs->spl_dcache);
	DQUEUE_FOREACH(&fs->dcache_inuse, d, struct DENTRY_CACHE_ITEM) {
		if (d->d_entry_inode != NULL)
			vfs_release_inode(d->d_entry_inode);
	}
	kfree(fs->dcache_buffer);
	fs->dcache_buffer = NULL;
	spinlock_unlock(&fs->spl_dcache);
}

/*
 *
 * Attempts to look up a given entry for a directory inode; 
 * 
 */
struct DENTRY_CACHE_ITEM*
dcache_find_item_or_add_pending(struct VFS_INODE* inode, const char* entry)
{
	struct VFS_MOUNTED_FS* fs = inode->fs;
	KASSERT(fs->dcache_buffer != NULL, "dcache pool not initialized");

retry:
	spinlock_lock(&fs->spl_dcache);
	/*
	 * XXX This is just a simple linear search which attempts to avoid
	 * overhead by moving recent entries to the start
	 */
	if (!DQUEUE_EMPTY(&fs->dcache_inuse)) {
		DQUEUE_FOREACH(&fs->dcache_inuse, d, struct DENTRY_CACHE_ITEM) {
			if (d->d_dir_inode != inode || strcmp(d->d_entry, entry))
				continue;

			/*
			 * Entry found; but it's quite possible that this inode is still pending;
			 * if that is the case, our caller should sleep and wait for the other
			 * caller to finish up.
			 */
			if (d->d_entry_inode == NULL && ((d->d_flags & DENTRY_FLAG_NEGATIVE) == 0)) {
				spinlock_unlock(&fs->spl_dcache);
				return NULL;
			}

			/*
			 * Now, we must increase the entry' backing inode refcount; the caller
			 * expects an inode that will not go away.
			 */
			if (d->d_entry_inode != NULL) {
				spinlock_lock(&d->d_entry_inode->spl_inode);
				KASSERT(d->d_entry_inode->refcount > 0, "inode from cache has no refs! (refcount is %u)", d->d_entry_inode->refcount);
				d->d_entry_inode->refcount++;
				spinlock_unlock(&d->d_entry_inode->spl_inode);
			}

			/*
			 * Push the the item to the head of the cache; we expect the caller to
			 * free it once done, which will decrease the refcount to 1, which is OK
			 * as only the cache owns it in such a case.
		 	 */
			DQUEUE_REMOVE(&fs->dcache_inuse, d);
			DQUEUE_ADD_HEAD(&fs->dcache_inuse, d);
			spinlock_unlock(&fs->spl_dcache);
			return d;
		}
	}

	/* Item was not found; try to get one from the freelist */
	struct DENTRY_CACHE_ITEM* d = NULL;
	if (!DQUEUE_EMPTY(&fs->dcache_free)) {
		/* Got one! */
		d = DQUEUE_HEAD(&fs->dcache_free);
		DQUEUE_POP_HEAD(&fs->dcache_free);
	} else {
		/* Freelist is empty; we need to sarifice an item from the cache */
		panic("XXX WRITEME");
	}

	/*
	 * If we still do not have an inode - well, the cache is fully in use and we
	 * have to try again later. But do so with debugging!
	 */
	if (d == NULL) {
		spinlock_unlock(&fs->spl_dcache);
		kprintf("dcache_find_item_or_add_pending(): cache full and no empty entries, retrying!\n");
		reschedule();
		goto retry;
	}

	/* Initialize the item */
	d->d_dir_inode = inode;
	d->d_entry_inode = NULL;
	d->d_flags = 0;
	strcpy(d->d_entry, entry);
	DQUEUE_ADD_TAIL(&fs->dcache_inuse, d);
	spinlock_unlock(&fs->spl_dcache);
	kprintf("dcache: got entry, d=%p, entry='%s'\n", d, entry);
	return d;
}

/* vim:set ts=2 sw=2: */
