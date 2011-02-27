/*
 * Ananas inode cache
 *
 * Inode caching is done on a per-filesystem basis, and currently using a
 * simple linear list. It should be unified to a single tree for all
 * filesystems one day, but this is complicated because FSOP's are
 * variable length.
 */
#include <ananas/types.h>
#include <ananas/error.h>
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

#define ICACHE_DEBUG

static char*
fsop_to_string(struct VFS_MOUNTED_FS* fs, void* fsop)
{
	static char out[128];
	char tmp[8];
	strcpy(out, "");
	for (int i = 0; i < fs->fs_fsop_size; i++) {
		sprintf(tmp, "%x", ((char*)fsop)[i]);
		if (i > 0)
			strcat(out, ",");
		strcat(out, tmp);
	}
	return out;
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

#ifdef ICACHE_DEBUG
static void
icache_sanity_check(struct VFS_MOUNTED_FS* fs)
{
	DQUEUE_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		DQUEUE_FOREACH(&fs->fs_icache_inuse, jj, struct ICACHE_ITEM) {
			if (ii == jj)
				continue;

			KASSERT(fsop_compare(fs, ii->fsop, jj->fsop) != 0, "duplicate fsop in cache");
			KASSERT(ii->inode != jj->inode, "duplicate inode in cache");
		}
	}
}
#endif

void
icache_ensure_inode_gone(struct VFS_INODE* inode)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	DQUEUE_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		KASSERT(ii->inode != inode, "removing inode %p still in cache", inode);
	}
}

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
	kprintf("icache_dump(): fs=%p\n", fs);
	int i = 0;
	DQUEUE_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		kprintf("icache_entry=%p, inode=%p, fsop=",ii, ii->inode);
		for (int i = 0; i < fs->fs_fsop_size; i++)
			kprintf("%x ", (unsigned char)ii->fsop[i]);
		kprintf("\n");
		if (ii->inode != NULL)
			vfs_dump_inode(ii->inode);
		i++;
	}		
	kprintf("icache_dump(): %u entries\n", i);
}

void
icache_destroy(struct VFS_MOUNTED_FS* fs)
{	
	panic("icache_destroy");
#if 0
	ICACHE_LOCK(fs);
	DQUEUE_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		if (ii->inode != NULL)
			vfs_deref_inode(ii->inode);
	}
	kfree(fs->fs_icache_buffer);
	fs->fs_icache_buffer = NULL;
	ICACHE_UNLOCK(fs);
#endif
}

/*
 * Searches find an inode in the cache; adds a pending entry if it's not found.
 * Will return the cache item; the inode is NULL if a pending item was added,
 * the result is NULL if a pending inode is already in the cache.
 * 
 */
static struct ICACHE_ITEM*
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
				kprintf("icache_find_item_or_add_pending(): pending item, waiting\n");
				panic("musn't happen");
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

			/*
			 * Push the the item to the head of the cache; we expect the caller to
			 * free it once done, which will decrease the refcount to 1, which is OK
			 * as only the cache owns it in such a case.
		 	 */
			DQUEUE_REMOVE(&fs->fs_icache_inuse, ii);
			DQUEUE_ADD_HEAD(&fs->fs_icache_inuse, ii);
			ICACHE_UNLOCK(fs);
			TRACE(VFS, INFO, "cache hit: fs=%p,fsop=% => ii=%p, inode=%p\n", fs, fsop_to_string(fs, fsop), ii, ii->inode);
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
			KASSERT(jj->inode->i_refcount > 0, "cached inode %p has no refs! (refcount is %u)", jj->inode, jj->inode->i_refcount);
			if (jj->inode->i_refcount > 1) {
				/* Not just in the cache, this one */
				INODE_UNLOCK(jj->inode);
				continue;
			}
			TRACE(VFS, INFO, "removing only-cache item, jj=%p, inode=%p", jj, jj->inode);

			/* Remove item from our cache */
			DQUEUE_REMOVE(&fs->fs_icache_inuse, jj);

			/* This item just has got to go */
			vfs_deref_inode_locked(jj->inode);

			/* No need to unlock the inode; the pointer is gone */
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
	TRACE(VFS, INFO, "cache miss: fs=%p, fsop=%s => ii=%p", fs, fsop_to_string(fs, fsop), ii);
	ii->inode = NULL;
	memcpy(ii->fsop, fsop, fs->fs_fsop_size);
	DQUEUE_ADD_TAIL(&fs->fs_icache_inuse, ii);
	ICACHE_UNLOCK(fs);
	return ii;
}

static void
icache_remove_pending(struct VFS_MOUNTED_FS* fs, struct ICACHE_ITEM* ii)
{
	panic("musn't happen");
	KASSERT(ii->inode == NULL, "removing an item that is not pending");

	ICACHE_LOCK(fs);
	DQUEUE_REMOVE(&fs->fs_icache_inuse, ii);
	DQUEUE_ADD_TAIL(&fs->fs_icache_free, ii);
	ICACHE_UNLOCK(fs);
}

static struct VFS_INODE*
vfs_alloc_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode;

	if (fs->fs_fsops->alloc_inode != NULL) {
		inode = fs->fs_fsops->alloc_inode(fs);
	} else {
		inode = vfs_make_inode(fs);
	}
	return inode;
}

/*
 * Retrieves an inode by fsop. The inode's refcount will always be incremented
 * so the caller is responsible for calling vfs_deref_inode() once it is done.
 */
errorcode_t
vfs_get_inode(struct VFS_MOUNTED_FS* fs, void* fsop, struct VFS_INODE** destinode)
{
	struct ICACHE_ITEM* ii = NULL;

	TRACE(VFS, FUNC, "fs=%p, fsop=%s", fs, fsop_to_string(fs, fsop));

	/*
	 * Wait until we obtain a cache spot - this waits for pending inodes to be
	 * finished up. By ensuring we fill the cache we ensure the inode can only
	 * exist a single time.
	 */
	while(1) {
		ii = icache_find_item_or_add_pending(fs, fsop);
		if (ii != NULL)
			break;
		TRACE(VFS, WARN, "fsop is already pending, waiting...");
		/* XXX There should be a wakeup signal of some kind */
		reschedule();
	}

	if (ii->inode != NULL) {
		/* Already have the inode cached -> return it (refcount will already be incremented) */
		*destinode = ii->inode;
		TRACE(VFS, INFO, "cache hit: fs=%p, fsop=%s => ii=%p,inode=%p", fs, fsop_to_string(fs, fsop), ii, ii->inode);
		return ANANAS_ERROR_OK;
	}

	/*
	 * Must read the inode; if this fails, we have to ask the cache to remove the
	 * pending item. Because multiple callers for the same FSOP will not reach
	 * this point (they keep rescheduling, waiting for us to deal with it)
	 */
	struct VFS_INODE* inode = vfs_alloc_inode(fs);
	if (inode == NULL) {
panic("fooo");
		icache_remove_pending(fs, ii);
		return ANANAS_ERROR(OUT_OF_HANDLES);
	}
	errorcode_t result = fs->fs_fsops->read_inode(inode, fsop);
	if (result != ANANAS_ERROR_NONE) {
panic("waah - %u", result);
		vfs_deref_inode(inode); /* throws it away */
		return result;
	}

	/*
	 * First of all, increment the inode's refcount so that it will not go away while in the cache;
	 * the caller has one ref, and the cache has the second.
	 */
	KASSERT(inode != NULL, "wtf");
	vfs_ref_inode(inode);
	ii->inode = inode;
	KASSERT(ii->inode->i_refcount == 2, "fresh inode refcount incorrect");
#ifdef ICACHE_DEBUG
	icache_sanity_check(fs);
#endif
	TRACE(VFS, INFO, "cache miss: fs=%p, fsop=%s => ii=%p,inode=%p", fs, fsop_to_string(fs, fsop), ii, inode);
	*destinode = inode;
	return ANANAS_ERROR_OK;
}

/* vim:set ts=2 sw=2: */
