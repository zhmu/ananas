/*
 * Ananas inode cache
 *
 * Inode caching is done on a per-filesystem basis, and currently using a
 * simple linear list. It should be unified to a single tree for all
 * filesystems one day, but this was complicated because FSOP's were
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
	mutex_lock(&(fs)->fs_icache_lock)
#define ICACHE_UNLOCK(fs) \
	mutex_unlock(&(fs)->fs_icache_lock)

#define INODE_ASSERT_SANE(i) \
	KASSERT((i)->i_refcount > 0, "referencing inode with no refs");

#undef ICACHE_DEBUG

#ifdef ICACHE_DEBUG
static void
icache_sanity_check(struct VFS_MOUNTED_FS* fs)
{
	LIST_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		LIST_FOREACH(&fs->fs_icache_inuse, jj, struct ICACHE_ITEM) {
			if (ii == jj)
				continue;

			KASSERT(ii->inum != jj->inum) != 0, "duplicate inum in cache (%lx)", ii->inum);
			KASSERT(ii->inode != jj->inode, "duplicate inode in cache");
		}
	}
}
#endif

void
icache_ensure_inode_gone(struct VFS_INODE* inode)
{
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	LIST_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		KASSERT(ii->inode != inode, "removing inode %p still in cache", inode);
	}
}

void
icache_init(struct VFS_MOUNTED_FS* fs)
{
	mutex_init(&fs->fs_icache_lock, "icache");
	LIST_INIT(&fs->fs_icache_inuse);
	LIST_INIT(&fs->fs_icache_free);

	/*
	 * Construct an empty cache; we do a single allocation and add the items one
	 * by one to the free list.
	 */
	fs->fs_icache_buffer = kmalloc(ICACHE_ITEMS_PER_FS * sizeof(struct ICACHE_ITEM));
	memset(fs->fs_icache_buffer, 0, ICACHE_ITEMS_PER_FS * sizeof(struct ICACHE_ITEM));
	addr_t icache_ptr = (addr_t)fs->fs_icache_buffer;
	for (int i = 0; i < ICACHE_ITEMS_PER_FS; i++) {
		LIST_APPEND(&fs->fs_icache_free, (struct ICACHE_ITEM*)icache_ptr);
		icache_ptr += sizeof(struct ICACHE_ITEM);
	}
}

void
icache_dump(struct VFS_MOUNTED_FS* fs)
{
	kprintf("icache_dump(): fs=%p\n", fs);
	int n = 0;
	LIST_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		kprintf("icache_entry=%p, inode=%p, inum=%lx\n", ii, ii->inum);
		if (ii->inode != NULL)
			vfs_dump_inode(ii->inode);
		n++;
	}               
	kprintf("icache_dump(): %u entries\n", n);
}

void
icache_destroy(struct VFS_MOUNTED_FS* fs)
{	
	panic("icache_destroy");
#if 0
	ICACHE_LOCK(fs);
	LIST_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		if (ii->inode != NULL)
			vfs_deref_inode(ii->inode);
	}
	kfree(fs->fs_icache_buffer);
	fs->fs_icache_buffer = NULL;
	ICACHE_UNLOCK(fs);
#endif
}

/* Do not hold the icache lock when calling this function */
static void
inode_deref_locked(struct VFS_INODE* inode)
{
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->i_refcount);
	if (--inode->i_refcount > 0) {
		/*
		 * Refcount isn't zero - this means we shouldn't throw the item away. However,
		 * if the inode has no more references _and_ only the cache owns it at this
		 * point, we should remove it from the cache.
		 */
		if (inode->i_sb.st_nlink != 0 || inode->i_refcount != 1) {
			INODE_UNLOCK(inode);
			return;
		}

		/* Inode should be removed and has only a single reference (the cache) - force it */
		inode->i_refcount--;
	}

	/*
	 * The inode is truly gone; this means we have to remove it from our cache as
	 * well. We do this right before destroying it because we use the cache to
	 * ensure we will never have multiple copies of the same inode.
	 */
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	int removed = 0;
	ICACHE_LOCK(fs);
	LIST_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		if (ii->inode != inode)
			continue;
		LIST_REMOVE(&fs->fs_icache_inuse, ii);
		LIST_APPEND(&fs->fs_icache_free, ii);
		removed++;
		break;
	}
	ICACHE_UNLOCK(fs);
	KASSERT(removed == 1, "inode %p not in cache?", inode);

	/* Throw away the filesystem-specific inode parts, if any */
	if (fs->fs_fsops->destroy_inode != NULL) {
		fs->fs_fsops->destroy_inode(inode);
	} else {
		vfs_destroy_inode(inode);
	}
}

void
vfs_deref_inode(struct VFS_INODE* inode)
{
	INODE_ASSERT_SANE(inode);
	INODE_LOCK(inode);
	inode_deref_locked(inode);
	/* INODE_UNLOCK(inode); - no need, inode_deref_locked() handles this if needed */
}

void
vfs_ref_inode(struct VFS_INODE* inode)
{
	KASSERT(inode != NULL, "reffing a null inode");
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->i_refcount);
	INODE_ASSERT_SANE(inode);

	INODE_LOCK(inode);
	KASSERT(inode->i_refcount > 0, "referencing a dead inode");
	inode->i_refcount++;
	INODE_UNLOCK(inode);
}

/* Removes old entries from the cache - icache must be locked */
static void
icache_purge_old_entries(struct VFS_MOUNTED_FS* fs)
{
	LIST_FOREACH_REVERSE_SAFE(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		/*
		 * Skip any pending items - we are not responsible for their cleanup (and
		 * to do so would be to introduce a race in vfs_read_inode() !
		 */
		if (ii->inode == NULL)
			continue;

		/*
		 * Purge the dentry for the given inode; this should free the refs up and
		 * allow us to throw the inode away.
		 */
		dcache_remove_inode(ii->inode);

		/*
		 * Lock the item; if the refcount is one, it means the cache is the
		 * sole owner and we can just grab the inode.
		 */
		INODE_LOCK(ii->inode);
		KASSERT(ii->inode->i_refcount > 0, "cached inode %p has no refs! (refcount is %u)", ii->inode, ii->inode->i_refcount);
		if (ii->inode->i_refcount > 1) {
			/* Not just in the cache, this one */
			INODE_UNLOCK(ii->inode);
			continue;
		}
		TRACE(VFS, INFO, "removing only-cache item, ii=%p, inode=%p", ii, ii->inode);

		/* Remove the inode; this will be the final reference, so the cache item will be removed */
		ICACHE_UNLOCK(fs);
		inode_deref_locked(ii->inode);
		ICACHE_LOCK(fs);
	}
}

/*
 * Searches find an inode in the cache; adds a pending entry if it's not found.
 * Will return the cache item; the inode is NULL if a pending item was added,
 * the result is NULL if a pending inode is already in the cache.
 * 
 */
static struct ICACHE_ITEM*
icache_lookup(struct VFS_MOUNTED_FS* fs, ino_t inum)
{
	KASSERT(fs->fs_icache_buffer != NULL, "icache pool not initialized");

	ICACHE_LOCK(fs);

	/*
	 * XXX This is just a simple linear search which attempts to avoid
	 * overhead by moving recent entries to the start
	 */
	LIST_FOREACH(&fs->fs_icache_inuse, ii, struct ICACHE_ITEM) {
		if (ii->inum != inum)
			continue;

		/*
		 * It's quite possible that this inode is still pending; if that
		 * is the case, our caller should sleep and wait for the other
		 * caller to finish up.
		 */
		if (ii->inode == NULL) {
			ICACHE_UNLOCK(fs);
			kprintf("icache_lookup(): pending item, waiting\n");
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
		LIST_REMOVE(&fs->fs_icache_inuse, ii);
		LIST_PREPEND(&fs->fs_icache_inuse, ii);
		ICACHE_UNLOCK(fs);
		TRACE(VFS, INFO, "cache hit: fs=%p, inum=%lx => ii=%p, inode=%p", fs, inum, ii, ii->inode);
		return ii;
	}

	/* Item was not found; try to get one from the freelist */
	struct ICACHE_ITEM* ii = NULL;
	while(ii == NULL) {
		if (!LIST_EMPTY(&fs->fs_icache_free)) {
			/* Got one! */
			ii = LIST_HEAD(&fs->fs_icache_free);
			LIST_POP_HEAD(&fs->fs_icache_free);
		} else {
			/* Freelist is empty; we need to sacrifice an item from the cache */
			icache_purge_old_entries(fs);
			/*
			 * XXX next condition is too harsh - we should wait until we have an
			 *     available inode here...
			 */
			KASSERT(!LIST_EMPTY(&fs->fs_icache_free), "icache still full after purge??");
		}
	}

	/* Initialize the item and place it at the head; it's most recently used after all */
	TRACE(VFS, INFO, "cache miss: fs=%p, inum=%lx => ii=%p", fs, inum, ii);
	ii->inode = NULL;
	ii->inum = inum;
	LIST_PREPEND(&fs->fs_icache_inuse, ii);
	ICACHE_UNLOCK(fs);
	return ii;
}

static void
icache_remove_pending(struct VFS_MOUNTED_FS* fs, struct ICACHE_ITEM* ii)
{
	panic("musn't happen");
	KASSERT(ii->inode == NULL, "removing an item that is not pending");

	ICACHE_LOCK(fs);
	LIST_REMOVE(&fs->fs_icache_inuse, ii);
	LIST_APPEND(&fs->fs_icache_free, ii);
	ICACHE_UNLOCK(fs);
}

static struct VFS_INODE*
vfs_alloc_inode(struct VFS_MOUNTED_FS* fs, ino_t inum)
{
	struct VFS_INODE* inode;

	if (fs->fs_fsops->alloc_inode != NULL) {
		inode = fs->fs_fsops->alloc_inode(fs, inum);
	} else {
		inode = vfs_make_inode(fs, inum);
	}
	return inode;
}

/*
 * Retrieves an inode by number - inode's refcount will always be incremented
 * so the caller is responsible for calling vfs_deref_inode() once it is done.
 */
errorcode_t
vfs_get_inode(struct VFS_MOUNTED_FS* fs, ino_t inum, struct VFS_INODE** destinode)
{
	struct ICACHE_ITEM* ii = NULL;

	TRACE(VFS, FUNC, "fs=%p, inum=%lx", fs, inum);

	/*
	 * Wait until we obtain a cache spot - this waits for pending inodes to be
	 * finished up. By ensuring we fill the cache we ensure the inode can only
	 * exist a single time.
	 */
	while(1) {
		ii = icache_lookup(fs, inum);
		if (ii != NULL)
			break;
		TRACE(VFS, WARN, "inode is already pending, waiting...");
		/* XXX There should be a wakeup signal of some kind */
		reschedule();
	}

	if (ii->inode != NULL) {
		/* Already have the inode cached -> return it (refcount will already be incremented) */
		*destinode = ii->inode;
		TRACE(VFS, INFO, "cache hit: fs=%p, inum=%lx => ii=%p,inode=%p", fs, inum, ii, ii->inode);
		return ananas_success();
	}

	/*
	 * Must read the inode; if this fails, we have to ask the cache to remove the
	 * pending item. Because multiple callers for the same inum will not reach
	 * this point (they keep rescheduling, waiting for us to deal with it)
	 */
	struct VFS_INODE* inode = vfs_alloc_inode(fs, inum);
	if (inode == NULL) {
		icache_remove_pending(fs, ii);
		return ANANAS_ERROR(OUT_OF_HANDLES);
	}

	errorcode_t result = fs->fs_fsops->read_inode(inode, inum);
	if (ananas_is_failure(result)) {
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
	TRACE(VFS, INFO, "cache miss: fs=%p, inum=%lx => ii=%p,inode=%p", fs, inum, ii, inode);
	*destinode = inode;
	return ananas_success();
}

struct VFS_INODE*
vfs_make_inode(struct VFS_MOUNTED_FS* fs, ino_t inum)
{
	auto inode = new VFS_INODE;

	/* Set up the basic inode information */
	memset(inode, 0, sizeof(*inode));
	mutex_init(&inode->i_mutex, "inode");
	inode->i_refcount = 1;
	inode->i_fs = fs;
	inode->i_inum = inum;
	/* Fill out the stat fields we can */
	inode->i_sb.st_dev = (dev_t)(uintptr_t)fs->fs_device;
	inode->i_sb.st_rdev = (dev_t)(uintptr_t)fs->fs_device;
	inode->i_sb.st_blksize = fs->fs_block_size;
	TRACE(VFS, INFO, "made inode inode=%p with refcount=%u", inode, inode->i_refcount);
	return inode;
}

void
vfs_dump_inode(struct VFS_INODE* inode)
{
	struct stat* sb = &inode->i_sb;
	kprintf("ino     = %u\n", sb->st_ino);
	kprintf("refcount= %u\n", inode->i_refcount);
	kprintf("mode    = 0x%x\n", sb->st_mode);
	kprintf("uid/gid = %u:%u\n", sb->st_uid, sb->st_gid);
	kprintf("size    = %u\n", (uint32_t)sb->st_size); /* XXX for now */
	kprintf("blksize = %u\n", sb->st_blksize);
}

void
vfs_destroy_inode(struct VFS_INODE* inode)
{
	KASSERT(inode->i_refcount == 0, "destroying inode which still has refs");
	kfree(inode);
	TRACE(VFS, INFO, "destroyed inode=%p", inode);
}

/* vim:set ts=2 sw=2: */
