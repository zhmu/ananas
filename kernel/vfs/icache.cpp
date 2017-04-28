#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/vfs.h>
#include <ananas/vfs/icache.h>
#include <ananas/mm.h>
#include <ananas/kdb.h>
#include <ananas/init.h>
#include <ananas/lock.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include "options.h"

TRACE_SETUP;

#define INODE_ASSERT_SANE(i) \
	KASSERT((i)->i_refcount > 0, "referencing inode with no refs");

#undef ICACHE_DEBUG

#ifdef ICACHE_DEBUG
static void
icache_sanity_check()
{
	LIST_FOREACH(&icache_inuse, ii, struct ICACHE_ITEM) {
		LIST_FOREACH(&icache_inuse, jj, struct ICACHE_ITEM) {
			if (ii == jj)
				continue;

			KASSERT(ii->inum != jj->inum) != 0, "duplicate inum in cache (%lx)", ii->inum);
			KASSERT(ii->inode != jj->inode, "duplicate inode in cache");
		}
	}
}
#endif

namespace {

mutex_t icache_mtx;
struct ICACHE_QUEUE	icache_inuse;
struct ICACHE_QUEUE	icache_free;

inline void icache_lock()
{
	mutex_lock(&icache_mtx);
}

inline void icache_unlock()
{
	mutex_unlock(&icache_mtx);
}

inline void icache_assert_locked()
{
	mutex_assert(&icache_mtx, MTX_LOCKED);
}

errorcode_t
icache_init()
{
	mutex_init(&icache_mtx, "icache");
	LIST_INIT(&icache_inuse);
	LIST_INIT(&icache_free);

	/*
	 * Construct an empty cache; we do a single allocation and add the items one
	 * by one to the free list.
	 */
	auto icache = static_cast<struct ICACHE_ITEM*>(kmalloc(ICACHE_ITEMS_PER_FS * sizeof(struct ICACHE_ITEM)));
	memset(icache, 0, ICACHE_ITEMS_PER_FS * sizeof(struct ICACHE_ITEM));
	for (int i = 0; i < ICACHE_ITEMS_PER_FS; i++, icache++)
		LIST_APPEND(&icache_free, icache);

	return ananas_success();
}

void
icache_purge_old_entries()
{
	icache_assert_locked();

	LIST_FOREACH_REVERSE_SAFE(&icache_inuse, ic, struct ICACHE_ITEM) {
		/*
		 * Skip any pending items (we are not responsible for their cleanup (and
		 * to do so would be to introduce a race in vfs_read_inode()) and anything
		 * that still has references.
		 */
		struct VFS_INODE* inode = ic->ic_inode;
		if (inode == NULL || inode->i_refcount > 0)
			continue;

		/*
		 * Purge the dentry for the given inode; this should free the refs up and
		 * allow us to throw the inode away.
		 */
		dcache_remove_inode(inode);

		// Throw the actual inode away
		INODE_LOCK(inode);
		struct VFS_MOUNTED_FS* fs = inode->i_fs;
		if (fs->fs_fsops->destroy_inode != NULL) {
			fs->fs_fsops->destroy_inode(inode);
		} else {
			vfs_destroy_inode(inode);
		}
		// No need to INODE_UNLOCK() as the inode is destroyed now

		// Add the entry back to the freelist
		LIST_REMOVE(&icache_inuse, ic);
		LIST_APPEND(&icache_free, ic);
	}
}

struct ICACHE_ITEM*
icache_find_item_to_use()
{
	icache_assert_locked();

	while(true) {
		if (!LIST_EMPTY(&icache_free)) {
			/* Got one! */
			struct ICACHE_ITEM* ic = LIST_HEAD(&icache_free);
			LIST_POP_HEAD(&icache_free);
			return ic;
		}

		/* Freelist is empty; we need to sacrifice an item from the cache */
		icache_purge_old_entries();
		/*
		 * XXX next condition is too harsh - we should wait until we have an
		 *     available inode here...
		 */
		KASSERT(!LIST_EMPTY(&icache_free), "icache still full after purge??");
	}

	// NOTREACHED
}

} // unnamed namespace

void
vfs_deref_inode(struct VFS_INODE* inode)
{
	KASSERT(inode != NULL, "dereffing a null inode");
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->i_refcount);
	INODE_ASSERT_SANE(inode);

	INODE_LOCK(inode);
	KASSERT(inode->i_refcount > 0, "dereffing inode %p with invalid refcount %d", inode, inode->i_refcount);
	--inode->i_refcount;
	INODE_UNLOCK(inode);

  // Never free the backing inode here - we don't have to! We will get rid of
  // it once we need a fresh inode (icache_purge_old_entries() does this)
}

void
vfs_ref_inode(struct VFS_INODE* inode)
{
	KASSERT(inode != NULL, "reffing a null inode");
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->i_refcount);
	INODE_ASSERT_SANE(inode);

	INODE_LOCK(inode);
	KASSERT(inode->i_refcount > 0, "referencing a dead inode");
	++inode->i_refcount;
	INODE_UNLOCK(inode);
}

/*
 * Searches find an inode in the cache; adds a pending entry if it's not found.
 * Will return the cache item; the inode is NULL if a pending item was added,
 * the result is NULL if a pending inode is already in the cache.
 *
 * On success, an extra ref to the inode will be added (for the caller to free)
 */
static struct ICACHE_ITEM*
icache_lookup(struct VFS_MOUNTED_FS* fs, ino_t inum)
{
	icache_lock();

	/*
	 * XXX This is just a simple linear search which attempts to avoid
	 * overhead by moving recent entries to the start
	 */
	LIST_FOREACH(&icache_inuse, ic, struct ICACHE_ITEM) {
		if (ic->ic_fs != fs || ic->ic_inum != inum)
			continue;

		/*
		 * It's quite possible that this inode is still pending; if that
		 * is the case, our caller should sleep and wait for the other
		 * caller to finish up.
		 */
		if (ic->ic_inode == NULL) {
			icache_unlock();
			kprintf("icache_lookup(): pending item, waiting\n");
			return NULL;
		}

		/*
		 * Now, we must increase the inode's refcount. It could be anything
		 * from zero upwards, so we can't use vfs_ref_inode() to ref it.
		 *
		 * Note that we cannot safely do this if we are dropping the icache lock,
		 * because this creates a race: the item may be removed while we are
		 * waiting for the inode lock.
		 */
		++ic->ic_inode->i_refcount;

		/*
		 * Push the the item to the head of the cache; we expect the caller to
		 * free it once done, which will decrease the refcount to 1, which is OK
		 * as only the cache owns it in such a case.
		 */
		LIST_REMOVE(&icache_inuse, ic);
		LIST_PREPEND(&icache_inuse, ic);
		icache_unlock();
		TRACE(VFS, INFO, "cache hit: fs=%p, inum=%lx => ic=%p, inode=%p", fs, inum, ic, ic->ic_inode);
		return ic;
	}

	/* Fetch a new item and place it at the head; it's most recently used after all */
	struct ICACHE_ITEM* ic = icache_find_item_to_use();
	TRACE(VFS, INFO, "cache miss: fs=%p, inum=%lx => ic=%p", fs, inum, ic);
	ic->ic_fs = fs;
	ic->ic_inode = NULL;
	ic->ic_inum = inum;
	LIST_PREPEND(&icache_inuse, ic);
	icache_unlock();
	return ic;
}

static void
icache_remove_pending(struct ICACHE_ITEM* ic)
{
	panic("musn't happen");
	KASSERT(ic->ic_inode == NULL, "removing an item that is not pending");

	icache_lock();
	LIST_REMOVE(&icache_inuse, ic);
	LIST_APPEND(&icache_free, ic);
	icache_unlock();
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
 * Retrieves an inode by number - on success, the owner will hold a reference.
 */
errorcode_t
vfs_get_inode(struct VFS_MOUNTED_FS* fs, ino_t inum, struct VFS_INODE** destinode)
{
	struct ICACHE_ITEM* ic = NULL;

	TRACE(VFS, FUNC, "fs=%p, inum=%lx", fs, inum);

	/*
	 * Wait until we obtain a cache spot - this waits for pending inodes to be
	 * finished up. By ensuring we fill the cache we ensure the inode can only
	 * exist a single time.
	 */
	while(1) {
		ic = icache_lookup(fs, inum);
		if (ic != NULL)
			break;
		TRACE(VFS, WARN, "inode is already pending, waiting...");
		/* XXX There should be a wakeup signal of some kind */
		reschedule();
	}

	if (ic->ic_inode != NULL) {
		/* Already have the inode cached -> return it (refcount will already be incremented) */
		*destinode = ic->ic_inode;
		TRACE(VFS, INFO, "cache hit: fs=%p, inum=%lx => ic=%p,inode=%p", fs, inum, ic, ic->ic_inode);
		return ananas_success();
	}

	/*
	 * Must read the inode; if this fails, we have to ask the cache to remove the
	 * pending item. Because multiple callers for the same inum will not reach
	 * this point (they keep rescheduling, waiting for us to deal with it)
	 */
	struct VFS_INODE* inode = vfs_alloc_inode(fs, inum);
	if (inode == NULL) {
		icache_remove_pending(ic);
		return ANANAS_ERROR(OUT_OF_HANDLES);
	}

	errorcode_t result = fs->fs_fsops->read_inode(inode, inum);
	if (ananas_is_failure(result)) {
		vfs_deref_inode(inode); /* throws it away */
		return result;
	}

	// Hook the inode to our cache
	ic->ic_inode = inode;
	KASSERT(ic->ic_inode->i_refcount == 1, "fresh inode refcount incorrect");
#ifdef ICACHE_DEBUG
	icache_sanity_check();
#endif
	TRACE(VFS, INFO, "cache miss: fs=%p, inum=%lx => ic=%p,inode=%p", fs, inum, ic, inode);
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
	kprintf("  ino     = %u\n", sb->st_ino);
	kprintf("  refcount= %u\n", inode->i_refcount);
	kprintf("  mode    = 0x%x\n", sb->st_mode);
	kprintf("  uid/gid = %u:%u\n", sb->st_uid, sb->st_gid);
	kprintf("  size    = %u\n", (uint32_t)sb->st_size); /* XXX for now */
	kprintf("  blksize = %u\n", sb->st_blksize);
}

void
vfs_destroy_inode(struct VFS_INODE* inode)
{
	KASSERT(inode->i_refcount == 0, "destroying inode which still has refs");
	kfree(inode);
	TRACE(VFS, INFO, "destroyed inode=%p", inode);
}

#ifdef OPTION_KDB
KDB_COMMAND(icache, NULL, "Show inode cache")
{
	int n = 0;
	LIST_FOREACH(&icache_inuse, ii, struct ICACHE_ITEM) {
		kprintf("icache_entry=%p, inode=%p, inum=%lx\n", ii, ii->ic_inode, ii->ic_inum);
		if (ii->ic_inode != NULL)
			vfs_dump_inode(ii->ic_inode);
		n++;
	}               
	kprintf("Inode cache contains %u entries\n", n);
}
#endif

INIT_FUNCTION(icache_init, SUBSYSTEM_VFS, ORDER_FIRST);

/* vim:set ts=2 sw=2: */
