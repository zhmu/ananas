/*
 * Ananas dentry cache (heavily based on icache.c)
 *
 * A 'dentry' is a directory entry, and can be seen as the function f:
 * directory_inode x entry_name -> inode.
 *
 * We try to keep as much entries in memory as possible, only overwriting
 * them if we really need to.
 */
#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/init.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "kernel/trace.h"
#include "kernel/vfs/types.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/mount.h"
#include "kernel/vfs/icache.h"
#include "options.h"

TRACE_SETUP;

namespace {

mutex_t dcache_mtx;
DEntryList dcache_inuse;
DEntryList dcache_free;

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

	/*
	 * Make an empty cache; we allocate one big pool and set up pointers to the
	 * items as necessary.
	 */
	{
		auto dentry = static_cast<DEntry*>(kmalloc(DCACHE_ITEMS_PER_FS * sizeof(DEntry)));
		memset(dentry, 0, DCACHE_ITEMS_PER_FS * sizeof(DEntry));
		for (int i = 0; i < DCACHE_ITEMS_PER_FS; i++, dentry++)
			dcache_free.push_back(*dentry);
	}

	return ananas_success();
}

DEntry*
dcache_find_entry_to_use()
{
	dcache_assert_locked();

	if (!dcache_free.empty()) {
			DEntry& d = dcache_free.front();
			dcache_free.pop_front();
			return &d;
	}

	/*
	 * Our dcache is ordered from old-to-new, so we'll start at the back and
	 * take anything which has no refs and isn't a root dentry.
	 */
	for(auto it = dcache_inuse.rbegin(); it != dcache_inuse.rend(); /* nothing */) {
		auto& d = *it; ++it;
		if (d.d_refcount == 0 && (d.d_flags & DENTRY_FLAG_ROOT) == 0) {
			// This dentry should be good to use - remove any backing inode it has,
			// as we will overwrite it
			if (d.d_inode != nullptr) {
				vfs_deref_inode(*d.d_inode);
				d.d_inode = nullptr;
			}

			dcache_inuse.remove(d);
			return &d;
		}
	}

	return nullptr;
}


} // unnamed namespace

static void dentry_deref_locked(DEntry& de);

DEntry&
dcache_create_root_dentry(struct VFS_MOUNTED_FS* fs)
{
	dcache_lock();

	DEntry* d = dcache_find_entry_to_use();
	KASSERT(d != nullptr, "out of dentries"); // XXX deal with this

	d->d_fs = fs;
	d->d_refcount = 1; /* filesystem itself */
	d->d_inode = NULL; /* supplied by the file system */
	d->d_flags = DENTRY_FLAG_ROOT;
	strcpy(d->d_entry, "/");
	dcache_inuse.push_front(*d);

	dcache_unlock();
	return *d;
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
DEntry*
dcache_lookup(DEntry& parent, const char* entry)
{
	TRACE(VFS, FUNC, "parent=%p, entry='%s'", &parent, entry);

	dcache_lock();

	/*
	 * XXX This is just a simple linear search which attempts to avoid
	 * overhead by moving recent entries to the start.
	 */
	for(auto& d: dcache_inuse) {
		if (d.d_parent != &parent || strcmp(d.d_entry, entry) != 0)
			continue;

		/*
		 * It's quite possible that this inode is still pending; if that is the
		 * case, our caller should sleep and wait for the other caller to finish
		 * up.
		 *
		 * XXX We shouldn't burden the caller with this!
		 */
		if (d.d_inode == nullptr && (d.d_flags & DENTRY_FLAG_NEGATIVE) == 0) {
			dcache_unlock();
			return nullptr;
		}

		// Add an extra ref to the dentry; we'll be giving it to the caller. Don't use dentry_ref()
		// here as the original refcount may be zero.
		++d.d_refcount;

		// Push the the item to the head of the cache
		dcache_inuse.remove(d);
		dcache_inuse.push_front(d);
		dcache_unlock();
		TRACE(VFS, INFO, "cache hit: parent=%p, entry='%s' => d=%p, d.inode=%p", &parent, entry, &d, d.d_inode);
		return &d;
	}

	// Item was not found; try to get one from the freelist
	DEntry* d = nullptr;
	while(d == nullptr) {
		/* We are out of dcache entries; we should remove some of the older entries */
		d = dcache_find_entry_to_use();
		/* XXX we should be able to cope with the next condition, somehow */
		KASSERT(d != nullptr, "dcache full");
	}

	/* Add an explicit ref to the parent dentry; it will be referenced by our new dentry */
	dentry_ref(parent);

	/* Initialize the item */
	memset(d, 0, sizeof *d);
	d->d_fs = parent.d_fs;
	d->d_refcount = 1; // the caller
	d->d_parent = &parent;
	d->d_inode = NULL;
	d->d_flags = 0;
	strcpy(d->d_entry, entry);
	dcache_inuse.push_front(*d);
	dcache_unlock();
	TRACE(VFS, INFO, "cache miss: parent=%p, entry='%s' => d=%p", &parent, entry, d);
	return d;
}

void
dcache_purge_old_entries()
{
	dcache_lock();
	for(auto it = dcache_inuse.begin(); it != dcache_inuse.end(); /* nothing */) {
		auto& d = *it; ++it;
		if (d.d_refcount > 0 || (d.d_flags & DENTRY_FLAG_ROOT))
			continue; // in use or root, skip

		// Get rid of any backing inode; this is why we are called
		if (d.d_inode != nullptr) {
			vfs_deref_inode(*d.d_inode);
			d.d_inode = nullptr;
		}

		dcache_inuse.remove(d);
		dcache_free.push_front(d);
	}
	dcache_unlock();
}

void
dcache_set_inode(DEntry& de, INode& inode)
{
#if 0
	/* XXX NOTYET - negative flag is cleared to keep the entry alive */
	KASSERT((de->d_flags & DENTRY_FLAG_NEGATIVE), "entry is not negative");
#endif

	/* If we already have an inode, deref it; we don't care about it anymore */
	if (de.d_inode != NULL)
		vfs_deref_inode(*de.d_inode);

	/* Increase the refcount - the cache will have a ref to the inode now */
	vfs_ref_inode(inode);
	de.d_inode = &inode;
	de.d_flags &= ~DENTRY_FLAG_NEGATIVE;
}

void
dentry_ref(DEntry& d)
{
	KASSERT(d.d_refcount > 0, "invalid refcount %d", d.d_refcount);
	d.d_refcount++;
}

static void
dentry_deref_locked(DEntry& d)
{
	KASSERT(d.d_refcount > 0, "invalid refcount %d", d.d_refcount);

	// Remove a reference; if this brings us to zero, we need to remove it
	if (--d.d_refcount > 0)
		return;

	// We do not free backing inodes here - the reason is that we don't know
	// how they are to be re-looked up.

	// Free our reference to the parent
	if (d.d_parent != nullptr) {
		dentry_deref_locked(*d.d_parent);
		d.d_parent = nullptr;
	}
}

void
dentry_unlink(DEntry& de)
{
	dcache_lock();
	de.d_flags |= DENTRY_FLAG_NEGATIVE;
	if (de.d_inode != nullptr)
		vfs_deref_inode(*de.d_inode);
  de.d_inode = nullptr;
	dcache_unlock();
}

void
dentry_deref(DEntry& de)
{
	dcache_lock();
	dentry_deref_locked(de);
	dcache_unlock();
}

void
dcache_purge()
{
	dcache_lock();
	dcache_unlock();
}

size_t
dentry_construct_path(char* dest, size_t n, DEntry& dentry)
{
	/*
	 * XXX This code looks quite messy due to all the checks - the reason is that it always wants
	 * to return the number of bytes that _would_ be needed, if we had unlimited space - this
	 * allows it to be used to determine the length needed.
	 *
	 * Also, the workaround when dentry is the root isn't very pretty...
	 */
	size_t len = 0;
	for(DEntry* d = &dentry; (d->d_flags & DENTRY_FLAG_ROOT) == 0; d = d->d_parent) {
		len += strlen(d->d_entry) + 1 /* extra / */ ;
	}

	// If we were handed only the root dentry, cope with that here
	if(len == 0)
		len = 1;

	// Now slice pieces back-to-front
	size_t cur_offset = len;
	DEntry* d = &dentry;
	do {
		size_t entry_len = strlen(d->d_entry);
		cur_offset -= entry_len;
		ssize_t piece_len = entry_len;
		if (piece_len + cur_offset > n)
			piece_len = n - cur_offset;
		if (piece_len > 0)
			memcpy(&dest[cur_offset], d->d_entry, piece_len);
		cur_offset--;
		if (cur_offset < n)
			dest[cur_offset] = '/';

		d = d->d_parent;
	} while (d != NULL && (d->d_flags & DENTRY_FLAG_ROOT) == 0);

	// Ensure we'll place a \0-terminator - if we didn't have enough space, we'll
	// overwrite whatever was there.
	if (n > 0)
		dest[len < n ? len : n - 1] = '\0';
	return len;
}


#ifdef OPTION_KDB
KDB_COMMAND(dcache, NULL, "Show dentry cache")
{
	/* XXX Don't lock; this is for debugging purposes only */
	int n = 0;
	for(auto& d: dcache_inuse) {
		kprintf("dcache_entry=%p, parent=%p, inode=%p, reverse name=%s[%d]",
		 &d, d.d_parent, d.d_inode, d.d_entry, d.d_refcount);
		for (DEntry* curde = d.d_parent; curde != NULL; curde = curde->d_parent)
			kprintf(",%s[%d]", curde->d_entry, curde->d_refcount);
		kprintf("',flags=0x%x, refcount=%d\n",
		 d.d_flags, d.d_refcount);
		n++;
	}
	kprintf("dentry cache contains %u entries\n", n);
}
#endif

INIT_FUNCTION(dcache_init, SUBSYSTEM_VFS, ORDER_FIRST);

/* vim:set ts=2 sw=2: */
