#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/init.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "kernel/schedule.h" // XXX
#include "kernel/trace.h"
#include "kernel/vmpage.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/icache.h"
#include "options.h"

TRACE_SETUP;

#define INODE_ASSERT_SANE(i) \
	KASSERT((i)->i_refcount > 0, "referencing inode with no refs"); \
	KASSERT(((i)->i_flags & INODE_FLAG_GONE) == 0, "referencing gone inode");

namespace {

#define ICACHE_ITEMS 32

LIST_DEFINE(INODE_LIST, struct VFS_INODE);

mutex_t icache_mtx;
struct INODE_LIST icache_inuse;
struct INODE_LIST icache_free;

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

	// Allocate inodes - we can set up some basic information here
	// XXX we could use a slab allocator for this
	{
		auto inode = static_cast<struct VFS_INODE*>(kmalloc(ICACHE_ITEMS * sizeof(struct VFS_INODE)));
		memset(inode, 0, ICACHE_ITEMS * sizeof(struct VFS_INODE));
		for (int i = 0; i < ICACHE_ITEMS; i++, inode++) {
			mutex_init(&inode->i_mutex, "inode");
			LIST_APPEND(&icache_free, inode);
		}
	}

	return ananas_success();
}

void
icache_purge_old_entries()
{
	icache_assert_locked();
	icache_unlock();

	/*
	 * Remove any stale entries from the dentry cache - this will release their
	 * inodes, which means we will be able to purge them from the icache.
	 */
	dcache_purge_old_entries();

	icache_lock();
	LIST_FOREACH_REVERSE_SAFE(&icache_inuse, inode, struct VFS_INODE) {
		/*
		 * Skip any pending items (we are not responsible for their cleanup (and
		 * to do so would be to introduce a race in vfs_read_inode()) and anything
		 * that still has references.
		 */
		INODE_LOCK(inode);
		if (inode->i_refcount > 0 || (inode->i_flags & INODE_FLAG_PENDING)) {
			INODE_UNLOCK(inode);
			continue;
		}

		// Throw the actual inode away
		struct VFS_MOUNTED_FS* fs = inode->i_fs;
		if (fs->fs_fsops->discard_inode != NULL)
			fs->fs_fsops->discard_inode(inode);

		inode->i_refcount = -1; // in case someone tries to use it
		inode->i_privdata = nullptr;
		inode->i_flags |= INODE_FLAG_GONE;
		INODE_UNLOCK(inode);

		// Move the inode to the freelist as we can re-use it again
		LIST_REMOVE(&icache_inuse, inode);
		LIST_APPEND(&icache_free, inode);
	}
}

struct VFS_INODE*
icache_find_item_to_use()
{
	icache_assert_locked();

	while(true) {
		if (!LIST_EMPTY(&icache_free)) {
			/* Got one! */
			struct VFS_INODE* inode = LIST_HEAD(&icache_free);
			LIST_POP_HEAD(&icache_free);
			return inode;
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
 * and the locked inode is returned.
 */
static struct VFS_INODE*
icache_lookup_locked(struct VFS_MOUNTED_FS* fs, ino_t inum)
{
	icache_lock();

	/*
	 * XXX This is just a simple linear search which attempts to avoid
	 * overhead by moving recent entries to the start
	 */
	LIST_FOREACH(&icache_inuse, inode, struct VFS_INODE) {
		if (inode->i_fs != fs || inode->i_inum != inum)
			continue;

		// We found the inode - lock it so that it won't go
		INODE_LOCK(inode);

		/*
		 * It's quite possible that this inode is still pending; if that
		 * is the case, our caller should sleep and wait for the other
		 * caller to finish up.
		 */
		if (inode->i_flags & INODE_FLAG_PENDING) {
			INODE_UNLOCK(inode);
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
		++inode->i_refcount;
		KASSERT(inode->i_refcount >= 1, "huh?");

		/*
		 * Push the the item to the head of the cache; we expect the caller to
		 * free it once done, which will decrease the refcount to zero, which is
		 * fine as we delay freeing inodes if possible.
		 */
		LIST_REMOVE(&icache_inuse, inode);
		LIST_PREPEND(&icache_inuse, inode);
		icache_unlock();
		TRACE(VFS, INFO, "cache hit: fs=%p, inum=%lx => inode=%p", fs, inum, inode);
		return inode;
	}

	/* Fetch a new item and place it at the head; it's most recently used after all */
	struct VFS_INODE* inode = icache_find_item_to_use();
	TRACE(VFS, INFO, "cache miss: fs=%p, inum=%lx => inode=%p", fs, inum, inode);
	LIST_PREPEND(&icache_inuse, inode);

	// Fill out some basic information
	INODE_LOCK(inode);
	inode->i_refcount = 1; // caller
	inode->i_flags = INODE_FLAG_PENDING;
	inode->i_fs = fs;
	inode->i_inum = inum;
	inode->i_sb.st_dev = (dev_t)(uintptr_t)fs->fs_device;
	inode->i_sb.st_rdev = (dev_t)(uintptr_t)fs->fs_device;
	inode->i_sb.st_blksize = fs->fs_block_size;
	icache_unlock();
	return inode;
}

/*
 * Retrieves an inode by number - on success, the owner will hold a reference.
 */
errorcode_t
vfs_get_inode(struct VFS_MOUNTED_FS* fs, ino_t inum, struct VFS_INODE** destinode)
{
	TRACE(VFS, FUNC, "fs=%p, inum=%lx", fs, inum);

	/*
	 * Wait until we obtain a cache spot - this waits for pending inodes to be
	 * finished up. By ensuring we fill the cache we ensure the inode can only
	 * exist a single time.
	 */
	struct VFS_INODE* inode = NULL;
	while(true) {
		inode = icache_lookup_locked(fs, inum);
		if (inode != NULL)
			break;
		TRACE(VFS, WARN, "inode is already pending, waiting...");
		/* XXX There should be a wakeup signal of some kind */
		reschedule();
	}
	KASSERT(inode->i_fs == fs, "wtf?");

	if ((inode->i_flags & INODE_FLAG_PENDING) == 0) {
		/* Already have the inode cached -> return it (refcount will already be incremented) */
		INODE_UNLOCK(inode);
		*destinode = inode;
		TRACE(VFS, INFO, "cache hit: fs=%p, inum=%lx => inode=%p", fs, inum, inode);
		return ananas_success();
	}

	/*
	 * Must read the inode; first, we need to set up the filesystem-specifics of the inode.
	 */
	errorcode_t result = ananas_success();
	if (fs->fs_fsops->prepare_inode != NULL)
		result = fs->fs_fsops->prepare_inode(inode);
	if (ananas_is_failure(result)) {
		INODE_UNLOCK(inode);
		vfs_deref_inode(inode); /* throws it away */
		return result;
	}

	/*
   * Read the inode - multiple callers for the same inum will not reach
   * this point (they keep rescheduling, waiting for us to deal with it)
	 */
	result = fs->fs_fsops->read_inode(inode, inum);
	if (ananas_is_failure(result)) {
		INODE_UNLOCK(inode);
		vfs_deref_inode(inode); /* throws it away */
		return result;
	}

	// Inode is complete
	KASSERT(inode->i_refcount == 1, "fresh inode refcount incorrect");
	TRACE(VFS, INFO, "cache miss: fs=%p, inum=%lx => inode=%p", fs, inum, inode);
	inode->i_flags &= ~INODE_FLAG_PENDING;
	INODE_UNLOCK(inode);
	*destinode = inode;
	return ananas_success();
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
	for(const auto& vp: inode->i_pages) {
		vmpage_dump(vp, "    ");
	}
}

#ifdef OPTION_KDB
KDB_COMMAND(icache, NULL, "Show inode cache")
{
	int n = 0;
	LIST_FOREACH(&icache_inuse, inode, struct VFS_INODE) {
		kprintf("inode=%p, inum=%lx\n", inode, inode->i_inum);
		if ((inode->i_flags & INODE_FLAG_PENDING) == 0)
			vfs_dump_inode(inode);
		n++;
	}               
	kprintf("Inode cache contains %u entries\n", n);
}
#endif

INIT_FUNCTION(icache_init, SUBSYSTEM_VFS, ORDER_FIRST);

/* vim:set ts=2 sw=2: */
