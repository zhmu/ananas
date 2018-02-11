#include <ananas/types.h>
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/schedule.h" // XXX
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/generic.h"
#include "kernel/vfs/mount.h"

TRACE_SETUP;

#define VFS_DEBUG_LOOKUP 0
#define VFS_MAX_SYMLINK_LOOPS 20

static void
vfs_make_file(struct VFS_FILE* file, DEntry& dentry)
{
	memset(file, 0, sizeof(struct VFS_FILE));

	INode& inode = *dentry.d_inode;
	file->f_dentry = &dentry;
	file->f_offset = 0;
	if (inode.i_iops->fill_file != NULL)
		inode.i_iops->fill_file(inode, file);
}

Result
vfs_open(const char* fname, DEntry* cwd, struct VFS_FILE* out, int lookup_flags)
{
	DEntry* dentry;
	RESULT_PROPAGATE_FAILURE(
		vfs_lookup(cwd, dentry, fname, lookup_flags)
	);

	vfs_make_file(out, *dentry);
	return Result::Success();
}

Result
vfs_close(struct VFS_FILE* file)
{
	if(file->f_dentry != nullptr)
		dentry_deref(*file->f_dentry);
	file->f_dentry = nullptr;
	file->f_device = nullptr;
	return Result::Success();
}

Result
vfs_read(struct VFS_FILE* file, void* buf, size_t* len)
{
	KASSERT(file->f_dentry != NULL || file->f_device != NULL, "vfs_read on nonbacked file");
	if (file->f_device != NULL) {
		/* Device */
		if (file->f_device->GetCharDeviceOperations() == NULL)
			return RESULT_MAKE_FAILURE(EINVAL);
		else {
			return file->f_device->GetCharDeviceOperations()->Read(buf, *len, 0);
		}
	}

	INode* inode = file->f_dentry->d_inode;
	if (inode == NULL || inode->i_iops == NULL)
		return RESULT_MAKE_FAILURE(EINVAL);

	if (!vfs_is_filesystem_sane(inode->i_fs))
		return RESULT_MAKE_FAILURE(EIO);

	if (S_ISLNK(inode->i_sb.st_mode)) {
		// Symbolic link
		if (inode->i_iops->read_link == NULL)
			return RESULT_MAKE_FAILURE(EINVAL);
		return inode->i_iops->read_link(*inode, static_cast<char*>(buf), len);
	}

	if (S_ISREG(inode->i_sb.st_mode)) {
		/* Regular file */
		if (inode->i_iops->read == NULL)
			return RESULT_MAKE_FAILURE(EINVAL);
		return inode->i_iops->read(file, buf, len);
	}

	if (S_ISDIR(inode->i_sb.st_mode)) {
		// Directory
		if (inode->i_iops->readdir == NULL)
			return RESULT_MAKE_FAILURE(EINVAL);
		return inode->i_iops->readdir(file, buf, len);
	}

	// What's this?
	return RESULT_MAKE_FAILURE(EINVAL);
}

Result
vfs_write(struct VFS_FILE* file, const void* buf, size_t* len)
{
	KASSERT(file->f_dentry != NULL || file->f_device != NULL, "vfs_write on nonbacked file");
	if (file->f_device != NULL) {
		/* Device */
		if (file->f_device == NULL || file->f_device->GetCharDeviceOperations() == NULL)
			return RESULT_MAKE_FAILURE(EINVAL);
		else
			return file->f_device->GetCharDeviceOperations()->Write(buf, *len, 0);
	}

	INode* inode = file->f_dentry->d_inode;
	if (inode == NULL || inode->i_iops == NULL)
		return RESULT_MAKE_FAILURE(EINVAL);

	if (S_ISDIR(inode->i_sb.st_mode)) {
		/* Directory */
		return RESULT_MAKE_FAILURE(EINVAL);
	}

	if (!vfs_is_filesystem_sane(inode->i_fs))
		return RESULT_MAKE_FAILURE(EIO);

	/* Regular file */
	if (inode->i_iops->write == NULL)
		return RESULT_MAKE_FAILURE(EINVAL);
	return inode->i_iops->write(file, buf, len);
}

Result
vfs_seek(struct VFS_FILE* file, off_t offset)
{
	if (file->f_dentry == NULL || file->f_dentry->d_inode == NULL)
		return RESULT_MAKE_FAILURE(EBADF);
	if (offset > file->f_dentry->d_inode->i_sb.st_size)
		return RESULT_MAKE_FAILURE(ERANGE);
	file->f_offset = offset;
	return Result::Success();
}

static Result
vfs_follow_symlink(DEntry*& curdentry)
{
	int symlink_loops = 0;
	while (S_ISLNK(curdentry->d_inode->i_sb.st_mode) && symlink_loops < VFS_MAX_SYMLINK_LOOPS) {
		INode& inode = *curdentry->d_inode;
		DEntry* followed_dentry;
		auto result = inode.i_iops->follow_link(inode, *curdentry, followed_dentry);
		dentry_deref(*curdentry); /* let go of the ref; we are done with it */
		if (result.IsFailure())
			return result;

		curdentry = followed_dentry;
		symlink_loops++;
	}

	if (symlink_loops == VFS_MAX_SYMLINK_LOOPS) {
		dentry_deref(*curdentry);
		return RESULT_MAKE_FAILURE(ELOOP);
	}
	return Result::Success();
}

/*
 * Internally used to perform a lookup from directory entry 'name' to an inode;
 * 'curdentry' is the dentry  to start the lookup relative to, or NULL to
 * start from the root. This fills out the final dcache entry of the lookup; if
 * the last entry of the inode was resolved, final will be non-zero.
 *
 * Note that destinode, if filled out, will always be referenced; it is the
 * responsibility of the caller to derefence it.
 */
static Result
vfs_lookup_internal(DEntry* curdentry, const char* name, DEntry*& ditem, bool& final, int flags)
{
	char tmp[VFS_MAX_NAME_LEN + 1];
#if VFS_DEBUG_LOOKUP
	kprintf("vfs_lookup_internal((): curdentry=(%p,inode %p,mode %x, entry='%s'), name=%s\n",
		curdentry, curdentry != NULL ? curdentry->d_inode : NULL,
		(curdentry != NULL && curdentry->d_inode != NULL) ? curdentry->d_inode->i_sb.st_mode : 0,
		curdentry != NULL ? curdentry->d_entry : "-", name);
#endif

	/* Start with a clean slate */
	final = false;
	ditem = nullptr;

	/*
	 * First of all, see if we need to lookup relative to the root; if so,
	 * we must update the current inode.
	 */
	if (curdentry == NULL || *name == '/') {
		struct VFS_MOUNTED_FS* fs = vfs_get_rootfs();
		if (fs == NULL)
			/* If there is no root filesystem, nothing to do */
			return RESULT_MAKE_FAILURE(ENOENT);
		if (*name == '/')
			name++;

		/* Start by looking up the root inode */
		curdentry = fs->fs_root_dentry;
		KASSERT(curdentry != NULL, "no root dentry");
	}

	/*
	 * Explicitely reference the dentry; this is normally done by the VFS lookup
	 * function when it returns an dentry, but we need some place to start. The
	 * added benefit is that we won't need any exceptions, as we can just free
	 * any inode that doesn't work.
	 */
	dentry_ref(*curdentry);

	/*
	 * Walk through the name to look up; for example, if it is 'a/b/c', we need
	 * to start with the 'a' part, when we find it look at the 'b' part etc.
	 *
	 * In order to do this, next_name is the complete piece to handle 'a/b/c' and
	 * next_lookup is the subpiece of that to handle during the first iteration
	 * ('a'). Once we run out of 'next_name', we are done.
	 */
	const char* next_name = name;
	const char* next_lookup;
	while (next_name != NULL && *next_name != '\0' /* for trailing /-es */) {
		/*
		 * Isolate the next part of the part we have to look up. Note that
		 * we consider the input dentry as const, so we can't mess with it;
		 * this is why we need to make copies in 'tmp'.
		 */
		char* ptr = strchr(next_name, '/');
		if (ptr != NULL) {
			/* There's a slash in the path - must lookup next part */
			strncpy(tmp, next_name, ptr - next_name);
			tmp[ptr - next_name] = '\0';
			next_name = ++ptr;
			next_lookup = tmp;
		} else {
			next_lookup = next_name;
			next_name = NULL;
			/* This has to be the final entry */
			final = true;
		}

		// Ensure the dentry points to something still sane
		if (!vfs_is_filesystem_sane(curdentry->d_fs)) {
			dentry_deref(*curdentry); /* let go of the ref; we are done with it */
			return RESULT_MAKE_FAILURE(EIO);
		}

		/*
		 * If the entry to find is '.', continue to the next one; we are already
		 * there.
		 */
		if (strcmp(next_lookup, ".") == 0)
			continue;

		/*
		 * We need to recurse - is this item a symlink? If so, we need to follow
		 * it.
		 */
		if (auto result = vfs_follow_symlink(curdentry); result.IsFailure())
				return result;

		/*
		 * We need to recurse; this can only be done if this is a directory, so
		 * refuse if this isn't the case.
		 */
		if (S_ISDIR(curdentry->d_inode->i_sb.st_mode) == 0) {
			dentry_deref(*curdentry); /* let go of the ref; we are done with it */
			return RESULT_MAKE_FAILURE(ENOENT);
		}

		/*
		 * See if the item is in the cache; we will add it otherwise since we we
		 * use the cache to look for items. Note that dcache_lookup() returns a
		 * _reffed_ dentry, which is why we don't take it ourselves.
		 */
		DEntry* dentry;
		while(1) {
			dentry = dcache_lookup(*curdentry, next_lookup);
			if (dentry != nullptr)
				break;
			TRACE(VFS, WARN, "dentry item is already pending, waiting...");
			/* XXX There should be a wakeup signal of some kind */
			reschedule();
		}
#if VFS_DEBUG_LOOKUP
		kprintf("partial lookup for %p:'%s' -> dentry %p (flags %u)", curdentry, next_lookup, dentry, dentry->d_flags);
#endif
		ditem = dentry;

		if (dentry->d_flags & DENTRY_FLAG_NEGATIVE) {
			/* Entry is in the cache as a negative entry; this means we can't find it */
			dentry_deref(*curdentry); /* release parent, we are done with it */
			KASSERT(dentry->d_inode == NULL, "negative lookup with inode?");
			TRACE(VFS, INFO, "bailing, found negative dentry for '%s', curdentry %p -> %p", next_lookup, curdentry, dentry);
			return RESULT_MAKE_FAILURE(ENOENT);
		}

		/* If the entry has a backing inode, we are done and can look up the next part */
		if (dentry->d_inode != NULL) {
			dentry_deref(*curdentry); /* release parent, we are done with it */
			curdentry = dentry; /* and start with the new parent; it's reffed by dcache_lookup() */
			continue;
		}

		/*
		 * If we got here, it means the new dcache entry doesn't have an inode
		 * attached to it; we need to read it.
		 */
		INode* inode;
		TRACE(VFS, INFO, "performing lookup from %p:'%s'", curdentry, next_lookup);
		Result result = curdentry->d_inode->i_iops->lookup(*curdentry, inode, next_lookup);
		dentry_deref(*curdentry); /* we no longer need it */
		if (result.IsSuccess()) {
			/*
			 * Lookup worked; we have a single-reffed inode now. We have to hook it
			 * up to the dentry cache, which can re-use the reference we have for it.
			 */
			dentry->d_inode = inode;
		} else {
			/* Lookup failed; make the entry cache negative */
			TRACE(VFS, INFO, "making negative dentry for %p:%s\n", curdentry, next_lookup);
			dentry->d_flags |= DENTRY_FLAG_NEGATIVE;
			/* No need to touch ditem; it'll be set already to the new dentry (and we can get to the parent from there) */
			return result;
		}

		/* Go one level deeper; we've already dereffed curdentry */
		curdentry = dentry;
	}

	// Handle with the last follow, if we need to
	if (curdentry != nullptr && (flags & VFS_LOOKUP_FLAG_NO_FOLLOW) == 0) {
		auto result = vfs_follow_symlink(curdentry);
		if (result.IsFailure())
			return result;
	}

	ditem = curdentry;
	return Result::Success();
}

/*
 * Called to perform a lookup from directory entry 'dentry' to an inode;
 * 'curinode' is the initial inode to start the lookup relative to, or NULL to
 * start from the root.
 */
Result
vfs_lookup(DEntry* parent, DEntry*& destentry, const char* dentry, int flags)
{
	bool final;
	Result result = vfs_lookup_internal(parent, dentry, destentry, final, flags);
	if (result.IsSuccess()) {
#if VFS_DEBUG_LOOKUP
		kprintf("vfs_lookup(): parent=%p,dentry='%s' okay -> dentry %p\n", parent, dentry, *destentry);
#endif
		return result;
	}

	/* Lookup failed; dereference the destination if necessary */
	if (destentry != nullptr)
		dentry_deref(*destentry);
	return result;
}

/*
 * Creates a new inode in parent; sets 'file' to the destination on success.
 */
Result
vfs_create(DEntry* parent, struct VFS_FILE* file, const char* dentry, int mode)
{
	DEntry* de;
	bool final;
	Result result = vfs_lookup_internal(parent, dentry, de, final, 0);
	if (result.IsSuccess() || result.AsErrno() != ENOENT) {
		/*
		 * A 'no file found' error is expected as we are creating the new file; we
		 * are using the lookup code in order to obtain the cache item entry.
	 	 */
		if (result.IsSuccess()) {
			/* The lookup worked?! The file already exists; cancel the ref */
			KASSERT(de->d_inode != NULL, "successful lookup without inode");
			dentry_deref(*de);
			/* Update the error code */
			result = RESULT_MAKE_FAILURE(EEXIST);
		}
		return result;
	}

	/* Request failed; if this wasn't the final entry, bail: path is not present */
	if (!final) {
		dentry_deref(*de);
		return result;
	}

	/*
	 * Excellent, the path works but the final entry doesn't. Mark the directory
	 * entry as pending (as we are creating it) - this prevents it from going
	 * away.
	 */
	de->d_flags &= ~DENTRY_FLAG_NEGATIVE;
	KASSERT(parent != NULL && parent->d_inode != NULL, "attempt to create entry without a parent inode");
	INode* parentinode = parent->d_inode;
	KASSERT(S_ISDIR(parentinode->i_sb.st_mode), "final entry isn't an inode");

	/* If the filesystem can't create inodes, assume the operation is faulty */
	if (parentinode->i_iops->create == NULL)
		return RESULT_MAKE_FAILURE(EINVAL);

	if (!vfs_is_filesystem_sane(parentinode->i_fs))
		return RESULT_MAKE_FAILURE(EIO);

	/* Dear filesystem, create a new inode for us */
	result = parentinode->i_iops->create(*parentinode, de, mode);
	if (result.IsFailure()) {
		/* Failure; remark the directory entry (we don't own it anymore) and report the failure */
		de->d_flags |= DENTRY_FLAG_NEGATIVE;
	} else {
		/* Success; report the inode we created */
		vfs_make_file(file, *de);
	}
	return result;
}

/*
 * Grows a file to a given size.
 */
Result
vfs_grow(struct VFS_FILE* file, off_t size)
{
	INode* inode = file->f_dentry->d_inode;
	KASSERT(inode->i_sb.st_size < size, "no need to grow");

	if (!vfs_is_filesystem_sane(inode->i_fs))
		return RESULT_MAKE_FAILURE(EIO);

	/* Allocate a dummy buffer with the file data */
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));

	/* Keep appending \0 until the file is done */
	off_t cur_offset = file->f_offset;
	file->f_offset = inode->i_sb.st_size;
	while (inode->i_sb.st_size < size) {
		size_t chunklen = (size - inode->i_sb.st_size) > sizeof(buffer) ? sizeof(buffer) : size - inode->i_sb.st_size;
		size_t len = chunklen;
		RESULT_PROPAGATE_FAILURE(
			vfs_write(file, buffer, &len)
		);
	}
	file->f_offset = cur_offset;
	return Result::Success();
}

Result
vfs_unlink(struct VFS_FILE* file)
{
	KASSERT(file->f_dentry != NULL, "unlink without dentry?");

	/* Unlink is relative to the parent; so we'll need to obtain it */
	DEntry* parent = file->f_dentry->d_parent;
	if (parent == NULL || parent->d_inode == NULL)
		return RESULT_MAKE_FAILURE(EINVAL);

	INode& inode = *parent->d_inode;
	if (inode.i_iops->unlink == NULL)
		return RESULT_MAKE_FAILURE(EINVAL);

	if (!vfs_is_filesystem_sane(inode.i_fs))
		return RESULT_MAKE_FAILURE(EIO);

	RESULT_PROPAGATE_FAILURE(
		inode.i_iops->unlink(inode, *file->f_dentry)
	);

	/*
	 * Inform the dentry cache; the unlink operation should have removed it
	 * from storage, but we need to make sure it cannot be found anymore.
	 */
	dentry_unlink(*file->f_dentry);
	return Result::Success();
}

Result
vfs_rename(struct VFS_FILE* file, DEntry* parent, const char* dest)
{
	KASSERT(file->f_dentry != NULL, "rename without dentry?");

	/*
	 * Renames are performed using the parent directory's inode - if our parent
	 * does not have a rename function, we can avoid looking up things.
	 */
	DEntry* parent_dentry = file->f_dentry->d_parent;
	if (parent_dentry == NULL || parent_dentry->d_inode == NULL ||
	    parent_dentry->d_inode->i_iops->rename == NULL)
		return RESULT_MAKE_FAILURE(EINVAL);

	/*
	 * Look up the new location; we need a dentry to the new location for this to
	 * work.
	 */
	DEntry* de;
	bool final;
	Result result = vfs_lookup_internal(parent, dest, de, final, 0);
	if (result.IsSuccess() || result.AsErrno() != ENOENT) {
		/*
		 * A 'no file found' error is expected as we are creating a new name here;
		 * we are using the lookup code in order to obtain the cache item entry.
	 	 */
		if (result.IsSuccess()) {
			/* The lookup worked?! The file already exists; cancel the ref */
			KASSERT(de->d_inode != NULL, "successful lookup without inode");
			dentry_deref(*de);
			/* Update the error code */
			result = RESULT_MAKE_FAILURE(EEXIST);
		}
		return result;
	}

	/* Request failed; if this wasn't the final entry, bail: parent path is not present */
	if (!final) {
		dentry_deref(*de);
		return result;
	}

	/* Sanity checks */
	KASSERT(de->d_parent != NULL, "found dest dentry without parent");
	KASSERT(de->d_parent->d_inode != NULL, "found dest dentry without inode");

	/*
	 * Okay, we need to rename file->f_dentry in parent_inode to de. First of all, ensure we
	 * don't cross any filesystem boundaries. We can most easily do this by checking whether
	 * the parent directories reside on the same backing filesystem.
	 */
	INode* parent_inode = parent_dentry->d_inode;
	INode* dest_inode = de->d_parent->d_inode;
	if (parent_inode->i_fs != dest_inode->i_fs) {
		dentry_deref(*de);
		return RESULT_MAKE_FAILURE(EXDEV);
	}

	/* All seems to be in order; ask the filesystem to deal with the change */
	result = parent_inode->i_iops->rename(*parent_inode, *file->f_dentry, *dest_inode, *de);
	if (result.IsFailure()) {
		/* If something went wrong, ensure to free the new dentry */
		dentry_deref(*de);
		return result;
	}

	/*
	 * This worked; we should hook the new dentry up and throw away the old one.
	 * No need to touch the refcount of the new dentry as we're giving our ref to
	 * the file.
	 */
	DEntry* old_dentry = file->f_dentry;
	file->f_dentry = de;
	dentry_deref(*old_dentry);
	return Result::Success();
}

/* vim:set ts=2 sw=2: */
