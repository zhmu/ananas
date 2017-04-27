/*
 * Ananas dentry cache (heavily based on icache.c)
 *
 * A 'dentry' is a directory entry, and can be seen as the function f:
 * directory_inode x entry_name -> inode. These are cached on a per-filesystem
 * basis (XXX this must change as we no longer have variable-length FSOP's)
 */
#include <ananas/types.h>
#include <ananas/vfs/core.h>
#include <ananas/vfs/dentry.h>
#include <ananas/error.h>
#include <ananas/init.h>
#include <ananas/mm.h>
#include <ananas/lock.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/lib.h>

TRACE_SETUP;

namespace {

mutex_t dcache_mtx;
struct DENTRY_QUEUE	dcache_inuse;
struct DENTRY_QUEUE	dcache_free;

inline void dcache_lock()
{
	mutex_lock(&dcache_mtx);
}

inline void dcache_unlock()
{
	mutex_unlock(&dcache_mtx);
}

inline void dcache_assert_locked()
{
	mutex_assert(&dcache_mtx, MTX_LOCKED);
}

errorcode_t
dcache_init()
{
	mutex_init(&dcache_mtx, "dcache");
	LIST_INIT(&dcache_inuse);
	LIST_INIT(&dcache_free);

	/*
	 * Make an empty cache; we allocate one big pool and set up pointers to the
	 * items as necessary.
	 */
	{
		auto dentry = static_cast<struct DENTRY*>(kmalloc(DCACHE_ITEMS_PER_FS * sizeof(struct DENTRY)));
		memset(dentry, 0, DCACHE_ITEMS_PER_FS * sizeof(struct DENTRY));
		for (int i = 0; i < DCACHE_ITEMS_PER_FS; i++, dentry++)
			LIST_APPEND(&dcache_free, dentry);
	}

	return ananas_success();
}

} // unnamed namespace

static void dentry_deref_locked(struct DENTRY* de);

struct DENTRY*
dcache_create_root_dentry(struct VFS_MOUNTED_FS* fs)
{
	dcache_lock();
	KASSERT(!LIST_EMPTY(&dcache_free), "out of dentries"); // XXX deal with this
	struct DENTRY* dentry = LIST_HEAD(&dcache_free);
	LIST_POP_HEAD(&dcache_free);

	dentry->d_fs = fs;
	dentry->d_refcount = 1; /* filesystem itself */
	dentry->d_inode = NULL; /* supplied by the file system */
	dentry->d_flags = DENTRY_FLAG_PERMANENT | DENTRY_FLAG_CACHED;
	LIST_PREPEND(&dcache_inuse, dentry);

	dcache_unlock();
	return dentry;
}

void
dcache_dump()
{
	/* XXX Don't lock; this is for debugging purposes only */
	int n = 0;
	kprintf("dcache_dump()\n");
	LIST_FOREACH(&dcache_inuse, d, struct DENTRY) {
		kprintf("dcache_entry=%p, parent=%p, inode=%p, reverse name=%s[%d]",
		 d, d->d_parent, d->d_inode, d->d_entry, d->d_refcount);
		for (struct DENTRY* curde = d->d_parent; curde != NULL; curde = curde->d_parent)
			kprintf(",%s[%d]", curde->d_entry, curde->d_refcount);
		kprintf("',flags=0x%x, refcount=%d\n",
		 d->d_flags, d->d_refcount);
		n++;
	}
	kprintf("dcache_dump(): %u entries\n", n);
}

// This purges an entry from the cache but *does not* alter the refcount
static void
dcache_purge_dentry_from_cache(struct DENTRY* d)
{
	dcache_assert_locked();

	TRACE(VFS, FUNC, "purging d=%p [%s] flags=%d refs=%d", d, d->d_entry, d->d_flags, d->d_refcount);
	KASSERT(d->d_flags & DENTRY_FLAG_CACHED, "dentry not cached");

	d->d_flags &= ~DENTRY_FLAG_CACHED;

	/* Free our dentry itself - it is is no longer cached */
	LIST_REMOVE(&dcache_inuse, d);
	LIST_APPEND(&dcache_free, d);
}

static void
dcache_purge_old_entries()
{
	dcache_assert_locked();

	/*
	 * Our dcache is ordered from old-to-new, so we'll start at the back and
	 * starting purging things - we'll only purge things that are in the cache.
	 */
	int num_removed = 0;
	LIST_FOREACH_REVERSE_SAFE(&dcache_inuse, d, struct DENTRY) {
		if ((d->d_flags & DENTRY_FLAG_CACHED) == 0)
			continue;
		if (d->d_refcount > 1)
			continue;

		/*
		 * This entry has a single ref, and the holder is the cache - it is safe to
		 * dereference it.
		 */
		dentry_deref_locked(d);
		num_removed++;
	}

	KASSERT(num_removed > 0, "no entries removed");
}

/*
 *
 * Attempts to look up a given entry for a parent dentry. Returns a referenced
 * dentry entry on success.
 *
 * The only way for this function to return NULL is that the lookup is
 * currently pending; this means the attempt to is be retried.
 *
 * Note that this function must be called with a referenced dentry to ensure it
 * will not go away. This ref is not touched by this function.
 */
struct DENTRY*
dcache_lookup(struct DENTRY* parent, const char* entry)
{
	TRACE(VFS, FUNC, "parent=%p, entry='%s'", parent, entry);

	dcache_lock();

	/*
	 * XXX This is just a simple linear search which attempts to avoid
	 * overhead by moving recent entries to the start
	 */
	LIST_FOREACH(&dcache_inuse, d, struct DENTRY) {
		if (d->d_parent != parent || strcmp(d->d_entry, entry) != 0)
			continue;

		/*
		 * It's quite possible that this inode is still pending; if that is the
		 * case, our caller should sleep and wait for the other caller to finish
		 * up.
		 */
		if (d->d_inode == nullptr && (d->d_flags & DENTRY_FLAG_NEGATIVE) == 0) {
			dcache_unlock();
			return nullptr;
		}

		// Add an extra ref to the dentry; we'll be giving it to the caller
		dentry_ref(d);

		/*
		 * Push the the item to the head of the cache; we expect the caller to
		 * free it once done, which will decrease the refcount to 1, which is OK
		 * as only the cache owns it in such a case.
		 */
		LIST_REMOVE(&dcache_inuse, d);
		LIST_PREPEND(&dcache_inuse, d);
		dcache_unlock();
		TRACE(VFS, INFO, "cache hit: parent=%p, entry='%s' => d=%p, d.inode=%p", parent, entry, d, d->d_inode);
		return d;
	}

	// Item was not found; try to get one from the freelist
	struct DENTRY* d = NULL;
	while(d == nullptr) {
		if (!LIST_EMPTY(&dcache_free)) {
			/* Got one! */
			d = LIST_HEAD(&dcache_free);
			LIST_POP_HEAD(&dcache_free);
		} else {
			/* We are out of dcache entries; we should remove some of the older entries */
			dcache_purge_old_entries();
			/* XXX we should be able to cope with the next condition, somehow */
			KASSERT(!LIST_EMPTY(&dcache_free), "dcache still full after purge!");
		}
	}

	/* Add an explicit ref to the parent dentry; it will be referenced by our new dentry */
	dentry_ref(parent);

	/* Initialize the item */
	memset(d, 0, sizeof *d);
	d->d_fs = parent->d_fs;
	d->d_refcount = 2; /* the caller + the cache */
	d->d_parent = parent;
	d->d_inode = NULL;
	d->d_flags = DENTRY_FLAG_CACHED;
	strcpy(d->d_entry, entry);
	LIST_PREPEND(&dcache_inuse, d);
	dcache_unlock();
	TRACE(VFS, INFO, "cache miss: parent=%p, entry='%s' => d=%p", parent, entry, d);
	return d;
}

void
dcache_remove_inode(struct VFS_INODE* inode)
{
#if 0
	struct VFS_MOUNTED_FS* fs = inode->i_fs;

	DCACHE_LOCK(fs);
	LIST_FOREACH_SAFE(&fs->fs_dcache_inuse, d, struct DENTRY) {
		/* Never touch pending entries; the lookup code deals with them */
		if (d->d_inode == NULL)
			continue;
		/* Free the inode */
		vfs_deref_inode(d->d_dir_inode);
		if (d->d_entry_inode != NULL)
			vfs_deref_inode(d->d_entry_inode);

		/* Remove from in-use and add to free list */
		LIST_REMOVE(&fs->fs_dcache_inuse, d);
		LIST_APPEND(&fs->fs_dcache_free, d);
		TRACE(VFS, INFO, "freed cache item: d=%p, entry='%s', dir_inode=%p, entry_inode=%p",
		 d, d->d_entry, d->d_dir_inode, d->d_entry_inode);
	}
	DCACHE_UNLOCK(fs);
#endif
}

void
dcache_set_inode(struct DENTRY* de, struct VFS_INODE* inode)
{
#if 0
	/* XXX NOTYET - negative flag is cleared to keep the entry alive */
	KASSERT((de->d_flags & DENTRY_FLAG_NEGATIVE), "entry is not negative");
#endif
	KASSERT(inode != NULL, "no inode given");

	/* If we already have an inode, deref it; we don't care about it anymore */
	if (de->d_inode != NULL)
		vfs_deref_inode(de->d_inode);

	/* Increase the refcount - the cache will have a ref to the inode now */
	vfs_ref_inode(inode);
	de->d_inode = inode;
	de->d_flags &= ~DENTRY_FLAG_NEGATIVE;
}

void
dentry_ref(struct DENTRY* d)
{
	KASSERT(d->d_refcount > 0, "invalid refcount %d", d->d_refcount);
	d->d_refcount++;
}

static void
dentry_deref_locked(struct DENTRY* d)
{
	KASSERT(d->d_refcount > 0, "invalid refcount %d", d->d_refcount);
	--d->d_refcount;

	/* If we still have references left, we are done */
	if (d->d_refcount > 0)
		return;

	/* We are about to destroy the item; if it's still in the cache, get rid of it */
	if (d->d_flags & DENTRY_FLAG_CACHED) {
		dcache_purge_dentry_from_cache(d);
	}

	/* If we have a backing inode, release it */
	if (d->d_inode != NULL)
		vfs_deref_inode(d->d_inode);

	/* Free our reference to the parent */
	if (d->d_parent != NULL)
		dentry_deref_locked(d->d_parent);
}

void
dentry_deref(struct DENTRY* de)
{
	dcache_lock();
	dentry_deref_locked(de);
	dcache_unlock();
}

void
dcache_purge_entry(struct DENTRY* d)
{
	dcache_lock();
	dcache_purge_dentry_from_cache(d);
	dcache_unlock();

	/* And throw away the cache's reference */
	dentry_deref(d);
}

INIT_FUNCTION(dcache_init, SUBSYSTEM_VFS, ORDER_FIRST);

/* vim:set ts=2 sw=2: */
