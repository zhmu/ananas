#include <ananas/types.h>
#include "kernel/init.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/schedule.h" // XXX
#include "kernel/trace.h"
#include "kernel/vmpage.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/icache.h"
#include "options.h"

TRACE_SETUP;

#define INODE_ASSERT_SANE(i) \
	KASSERT((i).i_refcount > 0, "referencing inode with no refs"); \
	KASSERT(((i).i_flags & INODE_FLAG_GONE) == 0, "referencing gone inode");

namespace {

constexpr size_t initialCacheItems = 32;
constexpr size_t growCacheIncrement = 2;

Mutex icache_mtx{"icache"};
util::List<INode> icache_inuse;
util::List<INode> icache_free;

inline void icache_lock()
{
	icache_mtx.Lock();
}

inline void icache_unlock()
{
	icache_mtx.Unlock();
}

inline void icache_assert_locked()
{
	icache_mtx.AssertLocked();
}

void
GrowCache(size_t numberOfItems)
{
	for (size_t i = 0; i < numberOfItems; i++)
		icache_free.push_back(*new INode);
}

bool
PurgeEntries()
{
	icache_assert_locked();
	icache_unlock();

	/*
	 * Remove any stale entries from the dentry cache - this will release their
	 * inodes, which means we will be able to purge them from the icache.
	 */
	dcache_purge_old_entries();

	int num_purged = 0;
	icache_lock();
	for(auto rit = icache_inuse.rbegin(); rit != icache_inuse.rend(); /* nothing */) {
		auto& inode = *rit; ++rit;

		/*
		 * Skip any pending items (we are not responsible for their cleanup (and
		 * to do so would be to introduce a race in vfs_read_inode()) and anything
		 * that still has references.
		 */
		inode.Lock();
		if (inode.i_refcount > 0 || (inode.i_flags & INODE_FLAG_PENDING)) {
			inode.Unlock();
			continue;
		}

		// Throw the actual inode away
		struct VFS_MOUNTED_FS* fs = inode.i_fs;
		if (fs->fs_fsops->discard_inode != NULL)
			fs->fs_fsops->discard_inode(inode);

		// Free pages belonging on the inode
		for(auto& vp: inode.i_pages)
			vp.Deref();
		inode.i_pages.clear();

		inode.i_refcount = -1; // in case someone tries to use it
		inode.i_privdata = nullptr;
		inode.i_flags |= INODE_FLAG_GONE;
		inode.Unlock();

		// Move the inode to the freelist as we can re-use it again
		icache_inuse.remove(inode);
		icache_free.push_back(inode);
		num_purged++;
	}

	return num_purged > 0;
}

INode&
icache_find_item_to_use()
{
	icache_assert_locked();

	while(true) {
		if (!icache_free.empty()) {
			/* Got one! */
			auto& inode = icache_free.front();
			icache_free.pop_front();
			return inode;
		}

		// Freelist is empty; we need to sacrifice an item from the cache
		if (PurgeEntries())
			continue;

		// Nothing available; allocate some extra items
		GrowCache(growCacheIncrement);
	}

	// NOTREACHED
}

} // unnamed namespace

void
vfs_deref_inode(INode& inode)
{
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", &inode, inode.i_refcount);
	INODE_ASSERT_SANE(inode);

	inode.Lock();
	KASSERT(inode.i_refcount > 0, "dereffing inode %p with invalid refcount %d", &inode, inode.i_refcount);
	--inode.i_refcount;
	inode.Unlock();

  // Never free the backing inode here - we don't have to! We will get rid of
  // it once we need a fresh inode (PurgeEntries() does this)
}

void
vfs_ref_inode(INode& inode)
{
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", &inode, inode.i_refcount);
	INODE_ASSERT_SANE(inode);

	inode.Lock();
	KASSERT(inode.i_refcount > 0, "referencing a dead inode");
	++inode.i_refcount;
	inode.Unlock();
}

/*
 * Searches find an inode in the cache; adds a pending entry if it's not found.
 * Will return the cache item; the inode is NULL if a pending item was added,
 * the result is NULL if a pending inode is already in the cache.
 *
 * On success, an extra ref to the inode will be added (for the caller to free)
 * and the locked inode is returned.
 */
static INode*
icache_lookup_locked(struct VFS_MOUNTED_FS* fs, ino_t inum)
{
	icache_lock();

	/*
	 * XXX This is just a simple linear search which attempts to avoid
	 * overhead by moving recent entries to the start
	 */
	for(auto& inode: icache_inuse) {
		if (inode.i_fs != fs || inode.i_inum != inum)
			continue;

		// We found the inode - lock it so that it won't go
		inode.Lock();

		/*
		 * It's quite possible that this inode is still pending; if that
		 * is the case, our caller should sleep and wait for the other
		 * caller to finish up.
		 */
		if (inode.i_flags & INODE_FLAG_PENDING) {
			inode.Unlock();
			icache_unlock();
			kprintf("icache_lookup(): pending item, waiting\n");
			return nullptr;
		}

		/*
		 * Now, we must increase the inode's refcount. It could be anything
		 * from zero upwards, so we can't use vfs_ref_inode() to ref it.
		 *
		 * Note that we cannot safely do this if we are dropping the icache lock,
		 * because this creates a race: the item may be removed while we are
		 * waiting for the inode lock.
		 */
		++inode.i_refcount;
		KASSERT(inode.i_refcount >= 1, "huh?");

		/*
		 * Push the the item to the head of the cache; we expect the caller to
		 * free it once done, which will decrease the refcount to zero, which is
		 * fine as we delay freeing inodes if possible.
		 */
		icache_inuse.remove(inode);
		icache_inuse.push_front(inode);
		icache_unlock();
		TRACE(VFS, INFO, "cache hit: fs=%p, inum=%lx => inode=%p", fs, inum, &inode);
		return &inode;
	}

	/* Fetch a new item and place it at the head; it's most recently used after all */
	auto& inode = icache_find_item_to_use();
	TRACE(VFS, INFO, "cache miss: fs=%p, inum=%lx => inode=%p", fs, inum, &inode);
	icache_inuse.push_front(inode);

	// Fill out some basic information
	inode.Lock();
	inode.i_refcount = 1; // caller
	inode.i_flags = INODE_FLAG_PENDING;
	inode.i_fs = fs;
	inode.i_inum = inum;
	inode.i_sb.st_dev = (dev_t)(uintptr_t)fs->fs_device;
	inode.i_sb.st_rdev = (dev_t)(uintptr_t)fs->fs_device;
	inode.i_sb.st_blksize = fs->fs_block_size;
	icache_unlock();
	return &inode;
}

/*
 * Retrieves an inode by number - on success, the owner will hold a reference.
 */
Result
vfs_get_inode(struct VFS_MOUNTED_FS* fs, ino_t inum, INode*& destinode)
{
	TRACE(VFS, FUNC, "fs=%p, inum=%lx", fs, inum);

	/*
	 * Wait until we obtain a cache spot - this waits for pending inodes to be
	 * finished up. By ensuring we fill the cache we ensure the inode can only
	 * exist a single time.
	 */
	INode* inode = nullptr;
	while(true) {
		inode = icache_lookup_locked(fs, inum);
		if (inode != nullptr)
			break;
		TRACE(VFS, WARN, "inode is already pending, waiting...");
		/* XXX There should be a wakeup signal of some kind */
		reschedule();
	}
	KASSERT(inode->i_fs == fs, "wtf?");

	if ((inode->i_flags & INODE_FLAG_PENDING) == 0) {
		/* Already have the inode cached -> return it (refcount will already be incremented) */
		inode->Unlock();
		destinode = inode;
		TRACE(VFS, INFO, "cache hit: fs=%p, inum=%lx => inode=%p", fs, inum, inode);
		return Result::Success();
	}

	/*
	 * Must read the inode; first, we need to set up the filesystem-specifics of the inode.
	 */
	Result result = Result::Success();
	if (fs->fs_fsops->prepare_inode != NULL)
		result = fs->fs_fsops->prepare_inode(*inode);
	if (result.IsFailure()) {
		inode->Unlock();
		vfs_deref_inode(*inode); /* throws it away */
		return result;
	}

	/*
   * Read the inode - multiple callers for the same inum will not reach
   * this point (they keep rescheduling, waiting for us to deal with it)
	 */
	result = fs->fs_fsops->read_inode(*inode, inum);
	if (result.IsFailure()) {
		inode->Unlock();
		vfs_deref_inode(*inode); /* throws it away */
		return result;
	}

	// Inode is complete
	KASSERT(inode->i_refcount == 1, "fresh inode refcount incorrect");
	TRACE(VFS, INFO, "cache miss: fs=%p, inum=%lx => inode=%p", fs, inum, inode);
	inode->i_flags &= ~INODE_FLAG_PENDING;
	inode->Unlock();
	destinode = inode;
	return Result::Success();
}

void
vfs_dump_inode(INode& inode)
{
	struct stat* sb = &inode.i_sb;
	kprintf("  ino     = %u\n", sb->st_ino);
	kprintf("  refcount= %u\n", inode.i_refcount);
	kprintf("  mode    = 0x%x\n", sb->st_mode);
	kprintf("  uid/gid = %u:%u\n", sb->st_uid, sb->st_gid);
	kprintf("  size    = %u\n", (uint32_t)sb->st_size); /* XXX for now */
	kprintf("  blksize = %u\n", sb->st_blksize);
	for(const auto& vp: inode.i_pages) {
		vp.Dump("    ");
	}
}

#ifdef OPTION_KDB
const kdb::RegisterCommand kdbICache("icache", "Show inode cache", [](int, const kdb::Argument*)
{
	int n = 0;
	for(auto& inode: icache_inuse) {
		kprintf("inode=%p, inum=%lx\n", &inode, inode.i_inum);
		if ((inode.i_flags & INODE_FLAG_PENDING) == 0)
			vfs_dump_inode(inode);
		n++;
	}
	kprintf("Inode cache contains %u entries\n", n);
});
#endif

namespace {

const init::OnInit initInodeCache(init::SubSystem::VFS, init::Order::First, []()
{
	/*
	 * Make an initial empty cache; we allocate one big pool and set up pointers to the
	 * items as necessary. We may choose to shrink/expand this pool later on.
	 */
	GrowCache(initialCacheItems);
});

} // unnamed namespace

/* vim:set ts=2 sw=2: */
