/*
 * Ananas inode cache
 *
 * Inode caching is done on a per-filesystem basis, and currently using a
 * simple linear list. It should be unified to a single tree for all
 * filesystems one day, but this is complicated because FSOP's are
 * variable length.
 */
#include <ananas/types.h>
#include <ananas/vfs.h>
#include <ananas/vfs/icache.h>
#include <ananas/mm.h>
#include <ananas/lock.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/lib.h>

TRACE_SETUP;

#define ICACHE_LOCK(fs) \
	spinlock_lock(&(fs)->fs_icache_lock);
#define ICACHE_UNLOCK(fs) \
	spinlock_unlock(&(fs)->fs_icache_lock);

void
icache_init(struct VFS_MOUNTED_FS* fs)
{
	spinlock_init(&fs->fs_icache_lock);
	DQUEUE_INIT(&fs->fs_icache_inuse);
	DQUEUE_INIT(&fs->fs_icache_free);

	/*
	 * Construct an empty cache; we do a single allocation and add the items one
	 * by one to the free list.
	 */
	fs->fs_icache_buffer = kmalloc(ICACHE_ITEMS_PER_FS * (sizeof(struct ICACHE_ITEM) + fs->fs_fsop_size));
	memset(fs->fs_icache_buffer, 0, ICACHE_ITEMS_PER_FS * (sizeof(struct ICACHE_ITEM) + fs->fs_fsop_size));
	addr_t icache_ptr = (addr_t)fs->fs_icache_buffer;
	for (int i = 0; i < ICACHE_ITEMS_PER_FS; i++) {
		DQUEUE_ADD_TAIL(&fs->fs_icache_free, (struct ICACHE_ITEM*)icache_ptr);
		icache_ptr += sizeof(struct ICACHE_ITEM) + fs->fs_fsop_size;
	}
}

void
icache_dump(struct VFS_MOUNTED_FS* fs)
{
	ICACHE_LOCK(fs);
	kprintf("icache_dump(): fs=%p\n", fs);
	DQUEUE_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		kprintf("icache_entry=%p, inode=%p\n",ii, ii->inode);
		if (ii->inode != NULL)
			vfs_dump_inode(ii->inode);
	}		
	ICACHE_UNLOCK(fs);
}

void
icache_destroy(struct VFS_MOUNTED_FS* fs)
{	
	ICACHE_LOCK(fs);
	DQUEUE_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		if (ii->inode != NULL)
			vfs_deref_inode(ii->inode);
	}
	kfree(fs->fs_icache_buffer);
	fs->fs_icache_buffer = NULL;
	ICACHE_UNLOCK(fs);
}

static int
fsop_compare(struct VFS_MOUNTED_FS* fs, void* fsop1, void* fsop2)
{
	uint8_t* f1 = fsop1;
	uint8_t* f2 = fsop2;

	/* XXX this is just a dumb big-endian compare for now */
	for (int i = fs->fs_fsop_size - 1; i >= 0; i--) {
		if (f1[i] < f2[i])
			return -1;
		else if (f1[i] > f2[i])
			return 1;
		/* else equal; continue */
	}
	return 0;
}

/*
 * Searches find an inode in the cache; adds a pending entry if it's not found.
 * Will return the cache item; the inode is NULL if a pending item was added,
 * the result is NULL if a pending inode is already in the cache.
 * 
 */
struct ICACHE_ITEM*
icache_find_item_or_add_pending(struct VFS_MOUNTED_FS* fs, void* fsop)
{
	KASSERT(fs->fs_icache_buffer != NULL, "icache pool not initialized");

retry:
	ICACHE_LOCK(fs);

	/*
	 * XXX This is just a simple linear search which attempts to avoid
	 * overhead by moving recent entries to the start
	 */
	if (!DQUEUE_EMPTY(&fs->fs_icache_inuse)) {
		DQUEUE_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
			if (fsop_compare(fs, fsop, ii->fsop) != 0)
				continue;

			/*
			 * It's quite possible that this inode is still pending; if that
		 	 * is the case, our caller should sleep and wait for the other
			 * caller to finish up.
			 */
			if (ii->inode == NULL) {
				ICACHE_UNLOCK(fs);
				return NULL;
			}

			/* Now, we must increase the inode's lock count. We know the inode was
			 * in the cache, so it's refcount must be at least one (it could not be
			 * removed in between as we hold the cache lock). We increment the
			 * refcount a second time, as it's now being given to the calling thread.
			 *
			 * Note that we cannot safely do this if we are dropping the icache lock,
			 * because this creates a race: the item may be removed while we are
			 * waiting for the inode lock.
			 */
			vfs_ref_inode(ii->inode);
			TRACE(VFS, INFO, "returning inode %p with refcount=%u", ii->inode, ii->inode->i_refcount);

			/*
			 * Push the the item to the head of the cache; we expect the caller to
			 * free it once done, which will decrease the refcount to 1, which is OK
			 * as only the cache owns it in such a case.
		 	 */
			DQUEUE_REMOVE(&fs->fs_icache_inuse, ii);
			DQUEUE_ADD_HEAD(&fs->fs_icache_inuse, ii);
			ICACHE_UNLOCK(fs);
			return ii;
		}
	}

	/* Item was not found; try to get one from the freelist */
	struct ICACHE_ITEM* ii = NULL;
	if (!DQUEUE_EMPTY(&fs->fs_icache_free)) {
		/* Got one! */
		ii = DQUEUE_HEAD(&fs->fs_icache_free);
		DQUEUE_POP_HEAD(&fs->fs_icache_free);
	} else {
		/* Freelist is empty; we need to sarifice an item from the cache */
		DQUEUE_FOREACH_REVERSE_SAFE(&fs->fs_icache_inuse, jj, struct ICACHE_ITEM) {
			/*
			 * Skip any pending items - we are not responsible for their cleanup (and
			 * to do so would be to introduce a race in vfs_read_inode() !
			 */
			if (jj->inode == NULL)
				continue;

			/*
			 * Lock the item; if the refcount is one, it means the cache is the
			 * sole owner and we can just grab the inode.
			 */
			INODE_LOCK(jj->inode);
			KASSERT(jj->inode->i_refcount > 0, "cached inode has no refs! (refcount is %u)", jj->inode->i_refcount);
			if (jj->inode->i_refcount > 1) {
				/* Not just in the cache, this one */
				INODE_UNLOCK(jj->inode);
				continue;
			}

			/* This item just has got to go */
			vfs_deref_inode_locked(jj->inode);

			/* Remove any references to this inode from the dentry cache */
			dentry_remove_inode(jj->inode);

			/* Remove item from our cache */
			DQUEUE_REMOVE(&fs->fs_icache_inuse, jj);

			/* Unlock the inode; it's can be used again */
			INODE_UNLOCK(jj->inode);
			ii = jj;
			break;
		}
	}

	/*
	 * If we still do not have an inode - well, the cache is fully in use and we
	 * have to try again later. But do so with debugging!
	 */
	if (ii == NULL) {
		ICACHE_UNLOCK(fs);
		kprintf("icache_find_item_or_add_pending(): cache full and no empty entries, retrying!\n");
		reschedule();
		goto retry;
	}

	/* Initialize the item */
	ii->inode = NULL;
	memcpy(ii->fsop, fsop, fs->fs_fsop_size);
	DQUEUE_ADD_TAIL(&fs->fs_icache_inuse, ii);
	ICACHE_UNLOCK(fs);
	return ii;
}

void
icache_remove_pending(struct VFS_MOUNTED_FS* fs, struct ICACHE_ITEM* ii)
{
	KASSERT(ii->inode == NULL, "removing an item that is not pending");

	ICACHE_LOCK(fs);
	DQUEUE_REMOVE(&fs->fs_icache_inuse, ii);
	DQUEUE_ADD_TAIL(&fs->fs_icache_free, ii);
	ICACHE_UNLOCK(fs);
}

void
icache_set_pending(struct VFS_MOUNTED_FS* fs, struct ICACHE_ITEM* ii, struct VFS_INODE* inode)
{
	KASSERT(ii->inode == NULL, "updating an item that is not pending");
	KASSERT(fs == inode->i_fs, "inode does not belong to this filesystem");

	/* First of all, increment the inode's refcount so that it will not go away while in the cache */
	vfs_ref_inode(inode);

	/* A lock is unnecessary here; the item will not be touched */
	ii->inode = inode;
}

void
icache_remove_inode(struct VFS_INODE* inode)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	KASSERT(inode->i_refcount == 0, "inode still referenced");

	ICACHE_LOCK(fs);
	if (!DQUEUE_EMPTY(&fs->fs_icache_inuse)) {
		DQUEUE_FOREACH_SAFE(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
			if (ii->inode != inode)
				continue;
			
			/* Remove from in-use and add to free list */
			DQUEUE_REMOVE(&fs->fs_icache_inuse, ii);
			DQUEUE_ADD_TAIL(&fs->fs_icache_free, ii);
			break; /* an entry is at most a single time in the inode cache */
		}
	}
	ICACHE_UNLOCK(fs);
}

/* vim:set ts=2 sw=2: */
