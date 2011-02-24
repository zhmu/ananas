/*
 * Ananas dentry cache (heavily based on icache.c)
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
#include <ananas/vfs/core.h>
#include <ananas/vfs/dentry.h>
#include <ananas/mm.h>
#include <ananas/lock.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/lib.h>

TRACE_SETUP;

#define DCACHE_LOCK(fs) \
	spinlock_lock(&(fs)->fs_dcache_lock);
#define DCACHE_UNLOCK(fs) \
	spinlock_unlock(&(fs)->fs_dcache_lock);

void
dcache_init(struct VFS_MOUNTED_FS* fs)
{
	spinlock_init(&fs->fs_dcache_lock);
	DQUEUE_INIT(&fs->fs_dcache_inuse);
	DQUEUE_INIT(&fs->fs_dcache_free);

	/*
	 * Make an empty cache; we allocate one big pool and set up pointers to the
	 * items as necessary.
	 */
	fs->fs_dcache_buffer = kmalloc(DCACHE_ITEMS_PER_FS * (sizeof(struct DENTRY_CACHE_ITEM) + fs->fs_fsop_size));
	memset(fs->fs_dcache_buffer, 0, DCACHE_ITEMS_PER_FS * (sizeof(struct DENTRY_CACHE_ITEM) + fs->fs_fsop_size));
	addr_t dcache_ptr = (addr_t)fs->fs_dcache_buffer;
	for (int i = 0; i < DCACHE_ITEMS_PER_FS; i++) {
		DQUEUE_ADD_TAIL(&fs->fs_dcache_free, (struct DENTRY_CACHE_ITEM*)dcache_ptr);
		dcache_ptr += sizeof(struct DENTRY_CACHE_ITEM) + fs->fs_fsop_size;
	}
}

void
dcache_dump(struct VFS_MOUNTED_FS* fs)
{
	DCACHE_LOCK(fs);
	kprintf("dcache_dump(): fs=%p\n", fs);
	DQUEUE_FOREACH(&fs->fs_dcache_inuse, d, struct DENTRY_CACHE_ITEM) {
		kprintf("dcache_entry=%p, dir_inode=%p, entry_inode=%p, name='%s', flags=0x%x\n",
		 d, d->d_dir_inode, d->d_entry_inode, d->d_entry, d->d_flags);
	}
	DCACHE_UNLOCK(fs);
}

void
dcache_destroy(struct VFS_MOUNTED_FS* fs)
{	
	DCACHE_LOCK(fs);
	DQUEUE_FOREACH(&fs->fs_dcache_inuse, d, struct DENTRY_CACHE_ITEM) {
		if (d->d_entry_inode != NULL)
			vfs_deref_inode(d->d_entry_inode);
	}
	kfree(fs->fs_dcache_buffer);
	fs->fs_dcache_buffer = NULL;
	DCACHE_UNLOCK(fs);
}

/*
 *
 * Attempts to look up a given entry for a directory inode; 
 * 
 */
struct DENTRY_CACHE_ITEM*
dcache_find_item_or_add_pending(struct VFS_INODE* inode, const char* entry)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	KASSERT(fs->fs_dcache_buffer != NULL, "dcache pool not initialized");

retry:
	DCACHE_LOCK(fs);

	/*
	 * XXX This is just a simple linear search which attempts to avoid
	 * overhead by moving recent entries to the start
	 */
	if (!DQUEUE_EMPTY(&fs->fs_dcache_inuse)) {
		DQUEUE_FOREACH(&fs->fs_dcache_inuse, d, struct DENTRY_CACHE_ITEM) {
			if (d->d_dir_inode != inode || strcmp(d->d_entry, entry))
				continue;

			/*
			 * Entry found; but it's quite possible that this inode is still pending;
			 * if that is the case, our caller should sleep and wait for the other
			 * caller to finish up.
			 */
			if (d->d_entry_inode == NULL && ((d->d_flags & DENTRY_FLAG_NEGATIVE) == 0)) {
				DCACHE_UNLOCK(fs);
				return NULL;
			}

			/*
			 * Now, we must increase the entry' backing inode refcount; the caller
			 * expects an inode that will not go away.
			 */
			if (d->d_entry_inode != NULL)
				vfs_ref_inode(d->d_entry_inode);

			/*
			 * Push the the item to the head of the cache; we expect the caller to
			 * free it once done, which will decrease the refcount to 1, which is OK
			 * as only the cache owns it in such a case.
		 	 */
			DQUEUE_REMOVE(&fs->fs_dcache_inuse, d);
			DQUEUE_ADD_HEAD(&fs->fs_dcache_inuse, d);
			DCACHE_UNLOCK(fs);
			return d;
		}
	}

	/* Item was not found; try to get one from the freelist */
	struct DENTRY_CACHE_ITEM* d = NULL;
	if (!DQUEUE_EMPTY(&fs->fs_dcache_free)) {
		/* Got one! */
		d = DQUEUE_HEAD(&fs->fs_dcache_free);
		DQUEUE_POP_HEAD(&fs->fs_dcache_free);
	} else {
		/* Freelist is empty; we need to sacrifice an item from the cache */
		DQUEUE_FOREACH_REVERSE_SAFE(&fs->fs_dcache_inuse, dd, struct DENTRY_CACHE_ITEM) {
			/* Skip permanent entries outright */
			if (dd->d_flags & DENTRY_FLAG_PERMANENT)
				continue;
			/* Skip pending entries; we're not responsible for their cleanup */
			if (dd->d_entry_inode == NULL)
				continue;

			/*
			 * See if the item has a refcount of just one, this indicates it's only
			 * in the cache and we can safely remove it then.
			 */
			struct VFS_INODE* inode = dd->d_entry_inode;
			INODE_LOCK(inode);
			KASSERT(inode->i_refcount > 0, "cached inode has no refs! (refcount is %u)", inode->i_refcount);
			if (inode->i_refcount > 1) {
				/* Refcount is more than one; can't use it */
				INODE_UNLOCK(inode);
				continue;
			}

			/* This item just has got to go */
			vfs_deref_inode_locked(inode);

			/* Remove any references to this inode from the inode cache */
			icache_remove_inode(inode);

			/* Throw away the backing inode and remove this item from the cache */
			DQUEUE_REMOVE(&fs->fs_dcache_inuse, dd);

			/* Unlock the inode; it's can be used again */
			spinlock_unlock(&inode->i_spinlock);
			d = dd;
			break;
		}
		/* Note that d can still be NULL if nothing could be removed */
	}

	/*
	 * If we still do not have an inode - well, the cache is fully in use and we
	 * have to try again later. But do so with debugging!
	 */
	if (d == NULL) {
		DCACHE_UNLOCK(fs);
		kprintf("dcache_find_item_or_add_pending(): cache full and no empty entries, retrying!\n");
		reschedule();
		goto retry;
	}

	/* Initialize the item */
	d->d_dir_inode = inode;
	d->d_entry_inode = NULL;
	d->d_flags = 0;
	strcpy(d->d_entry, entry);
	DQUEUE_ADD_TAIL(&fs->fs_dcache_inuse, d);
	DCACHE_UNLOCK(fs);
	TRACE(VFS, INFO, "dcache: got entry, d=%p, entry='%s'", d, entry);
	return d;
}

void
dentry_remove_inode(struct VFS_INODE* inode)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	KASSERT(inode->i_refcount == 0, "inode still referenced");

	DCACHE_LOCK(fs);
	if (!DQUEUE_EMPTY(&fs->fs_dcache_inuse)) {
		DQUEUE_FOREACH_SAFE(&fs->fs_dcache_inuse, d, struct DENTRY_CACHE_ITEM) {
			if (d->d_dir_inode != inode && d->d_entry_inode != inode)
				continue;
			/* Free the entry inode if we are removing its parent inode */
			if (d->d_dir_inode == inode && d->d_entry_inode != NULL)
				vfs_deref_inode(d->d_entry_inode);

			/* Remove from in-use and add to free list */
			DQUEUE_REMOVE(&fs->fs_dcache_inuse, d);
			DQUEUE_ADD_TAIL(&fs->fs_dcache_free, d);
		}
	}
	DCACHE_UNLOCK(fs);
}

/* vim:set ts=2 sw=2: */
