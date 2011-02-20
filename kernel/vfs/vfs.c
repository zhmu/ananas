#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>
#include "options.h"

TRACE_SETUP;

#define VFS_MOUNTED_FS_MAX	16

static struct SPINLOCK spl_mountedfs;
static struct VFS_MOUNTED_FS mountedfs[VFS_MOUNTED_FS_MAX];

void
vfs_init()
{
	memset(mountedfs, 0, sizeof(mountedfs));
	spinlock_init(&spl_mountedfs);
}

static struct VFS_MOUNTED_FS*
vfs_get_availmountpoint()
{
	spinlock_lock(&spl_mountedfs);
	for (unsigned int n = 0; n < VFS_MOUNTED_FS_MAX; n++) {
		struct VFS_MOUNTED_FS* fs = &mountedfs[n];
		if ((fs->flags & VFS_FLAG_INUSE) == 0) {
			fs->flags |= VFS_FLAG_INUSE;
			spinlock_unlock(&spl_mountedfs);
			return fs;
		}
	}
	spinlock_unlock(&spl_mountedfs);
	return NULL;
}

static struct VFS_MOUNTED_FS*
vfs_get_rootfs()
{
	spinlock_lock(&spl_mountedfs);
	for (unsigned int n = 0; n < VFS_MOUNTED_FS_MAX; n++) {
		struct VFS_MOUNTED_FS* fs = &mountedfs[n];
		if ((fs->flags & VFS_FLAG_INUSE) && (fs->mountpoint != NULL) && (strcmp(fs->mountpoint, "/") == 0)) {
			spinlock_unlock(&spl_mountedfs);
			return fs;
		}
	}
	spinlock_unlock(&spl_mountedfs);
	return NULL;
}

static struct VFS_FILESYSTEM_OPS*
vfs_get_fstype(const char* type)
{
	/* XXX This isn't perfect, but it will get the job done */
#ifdef EXT2FS
	extern struct VFS_FILESYSTEM_OPS fsops_ext2;
	if (!strcmp(type, "ext2")) return &fsops_ext2;
#endif
#ifdef ISO9660FS
	extern struct VFS_FILESYSTEM_OPS fsops_iso9660;
	if (!strcmp(type, "iso9660")) return &fsops_iso9660;
#endif
#ifdef CRAMFS
	extern struct VFS_FILESYSTEM_OPS fsops_cramfs;
	if (!strcmp(type, "cramfs")) return &fsops_cramfs;
#endif
#ifdef FATFS
	extern struct VFS_FILESYSTEM_OPS fsops_fat;
	if (!strcmp(type, "fatfs")) return &fsops_fat;
#endif
#ifdef DEVFS
	extern struct VFS_FILESYSTEM_OPS fsops_devfs;
	if (!strcmp(type, "devfs")) return &fsops_devfs;
#endif
	return NULL;
}

errorcode_t
vfs_mount(const char* from, const char* to, const char* type, void* options)
{
	errorcode_t err;
	struct VFS_FILESYSTEM_OPS* fsops = vfs_get_fstype(type);
	if (fsops == NULL)
		return ANANAS_ERROR(BAD_TYPE);
	struct VFS_MOUNTED_FS* fs = vfs_get_availmountpoint();
	if (fs == NULL)
		return ANANAS_ERROR(OUT_OF_HANDLES);

	/*
	 * First of all, do an explicit lookup for the mountpoint; this will result
	 * in the inode on the parent filesystem. Once the filesystem is mounted, we
	 * update the cache so that looking it up will work and thus, the inode can
	 * be used.
	 *
	 * XXX Note that we hack around to prevent doing this in the root case.
	 */
	struct DENTRY_CACHE_ITEM* d = NULL;
	if (strcmp(to, "/") != 0) {
		/* First of all, grab the 'to' path and chop the final part of */
		char tmp[VFS_MAX_NAME_LEN + 1];
		strcpy(tmp, to);
		char* ptr = strrchr(tmp, '/');
		KASSERT(ptr != NULL, "invalid path '%s'", to);
		*ptr = '\0';

		/* Look up the inode to the containing path */
		struct VFS_INODE* inode;
		err = vfs_lookup(NULL, &inode, tmp);
		if (err != ANANAS_ERROR_NONE) {
			memset(fs, 0, sizeof(*fs));
			return err;
		}
		/*
		 * XXX There is a race here; we must lock the cache item (but not the inode as
		 * we no longer need it)
		 */
		d = dcache_find_item_or_add_pending(inode, ptr + 1);
		vfs_release_inode(inode);
	}

	struct DEVICE* dev = NULL;
	if (from != NULL) {
		dev = device_find(from);
		if (dev == NULL)
			return ANANAS_ERROR(NO_FILE);
	}

	fs->device = dev;
	fs->fsops = fsops;
	icache_init(fs);
	dcache_init(fs);

	err = fs->fsops->mount(fs);
	if (err != ANANAS_ERROR_NONE) {
		dcache_destroy(fs);
		icache_destroy(fs);
		memset(fs, 0, sizeof(*fs));
		return err;
	}
	TRACE(VFS, INFO, "rootinode=%p", fs->root_inode);

	KASSERT(fs->root_inode != NULL, "successful mount without a root inode");
	KASSERT(S_ISDIR(fs->root_inode->sb.st_mode), "root inode isn't a directory");
	/* The refcount must be two: one for the fs->root_inode reference and one for the cache */
	KASSERT(fs->root_inode->refcount == 2, "bad refcount of root inode (must be 2, is %u)", fs->root_inode->refcount);
	fs->mountpoint = strdup(to);

	/*
	 * Override the previous inode with our root inode; this effectively hooks
	 * our filesystem to the parent.
	 */
	if (d != NULL) {
		if (d->d_entry_inode != NULL)
			vfs_release_inode(d->d_entry_inode);
		d->d_entry_inode = fs->root_inode;
		/* Ensure our entry will never be purged from the cache */
		d->d_flags |= DENTRY_FLAG_PERMANENT;
	}
	
	return ANANAS_ERROR_OK;
}

errorcode_t
vfs_unmount(const char* path)
{
	spinlock_lock(&spl_mountedfs);
	for (unsigned int n = 0; n < VFS_MOUNTED_FS_MAX; n++) {
		struct VFS_MOUNTED_FS* fs = &mountedfs[n];
		if ((fs->flags & VFS_FLAG_INUSE) && (fs->mountpoint != NULL) && (strcmp(fs->mountpoint, path) == 0)) {
			/* Got it; disown it immediately. XXX What about pending inodes etc? */
			fs->mountpoint = NULL;
			spinlock_unlock(&spl_mountedfs);
			/* XXX Ask filesystem politely to unmount */
			fs->flags = 0; /* Available */
			return ANANAS_ERROR_OK;
		}
	}
	spinlock_unlock(&spl_mountedfs);
	return ANANAS_ERROR(BAD_HANDLE); /* XXX */
}

struct VFS_INODE*
vfs_make_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode = kmalloc(sizeof(struct VFS_INODE));

	/* Set up the basic inode information */
	memset(inode, 0, sizeof(*inode));
	spinlock_init(&inode->spl_inode);
	inode->refcount++;
	inode->fs = fs;
	/* Fill out the stat fields we can */
	inode->sb.st_dev = (dev_t)fs->device;
	inode->sb.st_rdev = (dev_t)fs->device;
	inode->sb.st_blksize = fs->block_size;
	TRACE(VFS, INFO, "made inode inode=%p with refcount=%u", inode, inode->refcount);
	return inode;
}

void
vfs_dump_inode(struct VFS_INODE* inode)
{
	struct stat* sb = &inode->sb;
	kprintf("ino     = %u\n", sb->st_ino);
	kprintf("refcount= %u\n", inode->refcount);
	kprintf("mode    = 0x%x\n", sb->st_mode);
	kprintf("uid/gid = %u:%u\n", sb->st_uid, sb->st_gid);
	kprintf("size    = %u\n", (uint32_t)sb->st_size); /* XXX for now */
	kprintf("blksize = %u\n", sb->st_blksize);
}

void
vfs_destroy_inode(struct VFS_INODE* inode)
{
	kfree(inode);
}

static struct VFS_INODE*
vfs_alloc_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode;

	if (fs->fsops->alloc_inode != NULL) {
		inode = fs->fsops->alloc_inode(fs);
	} else {
		inode = vfs_make_inode(fs);
	}
	return inode;
}

void
vfs_release_inode_locked(struct VFS_INODE* inode)
{
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->refcount);
	if (--inode->refcount > 0) {
		/* Refcount isn't zero; don't throw the inode away */
		spinlock_unlock(&inode->spl_inode);
		return;
	}

	/* Note: we never unlock - the inode does not exist past here */
	if (inode->fs->fsops->alloc_inode != NULL) {
		inode->fs->fsops->destroy_inode(inode);
	} else {
		vfs_destroy_inode(inode);
	}
}

void
vfs_release_inode(struct VFS_INODE* inode)
{
	spinlock_lock(&inode->spl_inode);
	vfs_release_inode_locked(inode);
	spinlock_unlock(&inode->spl_inode);
}

struct BIO*
vfs_bread(struct VFS_MOUNTED_FS* fs, block_t block, size_t len)
{
	return bio_read(fs->device, block, len);
}

errorcode_t
vfs_get_inode(struct VFS_MOUNTED_FS* fs, void* fsop, struct VFS_INODE** destinode)
{
	
	struct ICACHE_ITEM* ii = NULL;

	/*
	 * Wait until we obtain a cache spot - this waits for pending inodes to be
	 * finished up.
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
		return ANANAS_ERROR_OK;
	}

	/*
	 * Must read the inode; if this fails, we have to ask the cache to remove the
	 * pending item. Because multiple callers for the same FSOP will not reach
	 * this point (they keep rescheduling, waiting for us to deal with it)
	 */
	struct VFS_INODE* inode = vfs_alloc_inode(fs);
	if (inode == NULL) {
		icache_remove_pending(fs, ii);
		return ANANAS_ERROR(OUT_OF_HANDLES);
	}
	errorcode_t result = fs->fsops->read_inode(inode, fsop);
	if (result != ANANAS_ERROR_NONE) {
		vfs_release_inode(inode);
		return result;
	}
	icache_set_pending(fs, ii, inode);
	ii->inode = inode;
	*destinode = inode;
	return ANANAS_ERROR_OK;
}

/*
 * Called to perform a lookup from directory entry 'dentry' to an inode;
 * 'curinode' is the initial inode to start the lookup relative to, or NULL to
 * start from the root.
 */
errorcode_t
vfs_lookup(struct VFS_INODE* curinode, struct VFS_INODE** destinode, const char* dentry)
{
	char tmp[VFS_MAX_NAME_LEN + 1];

	/*
	 * First of all, see if we need to lookup relative to the root; if so,
	 * we must update the current inode.
	 */
	if (curinode == NULL || *dentry == '/') {
		struct VFS_MOUNTED_FS* fs = vfs_get_rootfs();
		if (fs == NULL)
			/* If there is no root filesystem, nothing to do */
			return ANANAS_ERROR(NO_FILE);
		if (*dentry == '/')
			dentry++;

		/* Start by looking up the root inode */
		curinode = fs->root_inode;
		KASSERT(curinode != NULL, "no root inode");
	}

	/* Bail if there is no current inode; this makes the caller's path easier */
	if (curinode == NULL)
		return ANANAS_ERROR(NO_FILE);

	/*
	 * Explicitely reference the inode; this is normally done by the VFS lookup
	 * function when it returns an inode, but we need some place to start. The
	 * added benefit is that we won't need any exceptions, as we can just free
	 * any inode that doesn't work.
	 */
	vfs_ref_inode(curinode);

	const char* curdentry = dentry;
	const char* curlookup;
	while (curdentry != NULL && *curdentry != '\0' /* for trailing /-es */) {
		/*
		 * Isolate the next part of the part we have to look up. Note that
		 * we consider the input dentry as const, so we can't mess with it;
		 * this is why we need to make copies in 'tmp'.
		 */
		char* ptr = strchr(curdentry, '/');
		if (ptr != NULL) {
			/* There's a slash in the path - must lookup next part */
			strncpy(tmp, curdentry, ptr - curdentry);
			curdentry = ++ptr;
			curlookup = tmp;
		} else {
			curlookup = curdentry;
			curdentry = NULL;
		}

		/*
		 * If the entry to find is '.', continue to the next one; we are already
		 * there.
		 */
		if (strcmp(curlookup, ".") == 0)
			continue;

		/*
		 * We need to recurse; this can only be done if this is a directory, so
		 * refuse if this isn't the case.
		 */
		if (S_ISDIR(curinode->sb.st_mode) == 0) {
			vfs_release_inode(curinode);
			return ANANAS_ERROR(NO_FILE);
		}

		/*
		 * See if the item is in the cache; we will add it regardless.
		 * finished up.
		 */
		struct DENTRY_CACHE_ITEM* dentry;
		while(1) {
			dentry = dcache_find_item_or_add_pending(curinode, curlookup);
			if (dentry != NULL)
				break;
			TRACE(VFS, WARN, "dentry item is already pending, waiting...");
			/* XXX There should be a wakeup signal of some kind */
			reschedule();
		}

		if (dentry->d_flags & DENTRY_FLAG_NEGATIVE) {
			/* Entry is in the cache but cannot be found */
			return ANANAS_ERROR(NO_FILE);
		}

		if (dentry->d_entry_inode != NULL) {
			/* Already have the inode cached; release the previous inode */
			vfs_release_inode(curinode);
			/* And return the entry from the cache (refcount will already be incremented) */
			curinode = dentry->d_entry_inode;
			continue;
		}

		/*
	 	 * Attempt to look up whatever entry we need. Once this is done, we can
		 * get rid of the current lookup inode (as it won't be the inode itself
		 * since we special-case "." above).
		 */
		struct VFS_INODE* inode;
		errorcode_t err = curinode->iops->lookup(curinode, &inode, curlookup);
		vfs_release_inode(curinode);
		if (err == ANANAS_ERROR_NONE) {
			/*
			 * Lookup worked - if we got here, we have to hook the inode we found up
			 * to the dentry cache.
			 */
			dentry->d_entry_inode = inode;
		} else {
			/* Lookup failed; make the entry cache negative */
			dentry->d_flags |= DENTRY_FLAG_NEGATIVE;
			return err;
		}
		
		curinode = inode;
	}
	*destinode = curinode;
	return ANANAS_ERROR_OK;
}

errorcode_t
vfs_open(const char* fname, struct VFS_INODE* cwd, struct VFS_FILE* file)
{
	struct VFS_INODE* inode;
	errorcode_t err = vfs_lookup(cwd, &inode, fname);
	ANANAS_ERROR_RETURN(err);

	memset(file, 0, sizeof(struct VFS_FILE));
	file->inode = inode;
	file->offset = 0;
	return ANANAS_ERROR_OK;
}

errorcode_t
vfs_close(struct VFS_FILE* file)
{
	if(file->inode != NULL)
		vfs_release_inode(file->inode);
	file->inode = NULL; file->device = NULL;
	return ANANAS_ERROR_OK;
}

errorcode_t
vfs_read(struct VFS_FILE* file, void* buf, size_t* len)
{
	KASSERT(file->inode != NULL || file->device != NULL, "vfs_read on nonbacked file");
	if (file->device != NULL) {
		/* Device */
		if (file->device->driver == NULL || file->device->driver->drv_read == NULL)
			return ANANAS_ERROR(BAD_OPERATION);
		else {
			return file->device->driver->drv_read(file->device, buf, len, 0);
		}
	}

	if (file->inode == NULL || file->inode->iops == NULL)
		return ANANAS_ERROR(BAD_OPERATION);

	if (!S_ISDIR(file->inode->sb.st_mode)) {
		/* Regular file */
		if (file->inode->iops->read == NULL)
			return ANANAS_ERROR(BAD_OPERATION);
		return file->inode->iops->read(file, buf, len);
	}

	/* Directory */
	if (file->inode->iops->readdir == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return file->inode->iops->readdir(file, buf, len);
}

errorcode_t
vfs_write(struct VFS_FILE* file, const void* buf, size_t* len)
{
	KASSERT(file->inode != NULL || file->device != NULL, "vfs_write on nonbacked file");
	if (file->device != NULL) {
		/* Device */
		if (file->device->driver == NULL || file->device->driver->drv_write == NULL)
			return ANANAS_ERROR(BAD_OPERATION);
		else
			return file->device->driver->drv_write(file->device, buf, len, 0);
	}

	if (file->inode == NULL || file->inode->iops == NULL)
		return ANANAS_ERROR(BAD_OPERATION);

	if (S_ISDIR(file->inode->sb.st_mode)) {
		/* Directory */
		return ANANAS_ERROR(BAD_OPERATION);
	}

	/* Regular file */
	if (file->inode->iops->write == NULL)
		return ANANAS_ERROR(BAD_OPERATION);
	return file->inode->iops->write(file, buf, len);
}

errorcode_t
vfs_seek(struct VFS_FILE* file, off_t offset)
{
	if (file->inode == NULL)
		return ANANAS_ERROR(BAD_HANDLE);
	if (offset > file->inode->sb.st_size)
		return ANANAS_ERROR(BAD_RANGE);
	file->offset = offset;
	return ANANAS_ERROR_OK;
}

size_t
vfs_filldirent(void** dirents, size_t* size, const void* fsop, int fsoplen, const char* name, int namelen)
{
	/*
	 * First of all, ensure we have sufficient space. Note that we use the fact
	 * that de_fsop is only a single byte; we count it as the \0 byte.
	 */
	int de_length = sizeof(struct VFS_DIRENT) + fsoplen + namelen;
	if (*size < de_length)
		return 0;
	*size -= de_length;

	struct VFS_DIRENT* de = (struct VFS_DIRENT*)*dirents;
	*dirents += de_length;

	de->de_flags = 0; /* TODO */
	de->de_fsop_length = fsoplen;
	de->de_name_length = namelen;
	memcpy(de->de_fsop, fsop, fsoplen);
	memcpy(de->de_fsop + fsoplen, name, namelen);
	de->de_fsop[fsoplen + namelen] = '\0'; /* zero terminate */
	return de_length;
}

void
vfs_ref_inode(struct VFS_INODE* inode)
{
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->refcount);
	spinlock_lock(&inode->spl_inode);
	KASSERT(inode->refcount > 0, "referencing a dead inode");
	inode->refcount++;
	spinlock_unlock(&inode->spl_inode);
}

#ifdef KDB
void
kdb_cmd_vfs(int num_args, char** arg)
{
	spinlock_lock(&spl_mountedfs);
	for (unsigned int n = 0; n < VFS_MOUNTED_FS_MAX; n++) {
		struct VFS_MOUNTED_FS* fs = &mountedfs[n];
		if ((fs->flags & VFS_FLAG_INUSE) == 0)
			continue;
		kprintf(">> vfs=%p, flags=0x%x, mountpoint='%s'\n", fs, fs->flags, fs->mountpoint);
		icache_dump(fs);
		dcache_dump(fs);
	}
	spinlock_unlock(&spl_mountedfs);
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
