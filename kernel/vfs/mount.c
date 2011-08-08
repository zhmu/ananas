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
static spinlock_t spl_mountedfs = SPINLOCK_DEFAULT_INIT;
static struct VFS_MOUNTED_FS mountedfs[VFS_MOUNTED_FS_MAX];

static spinlock_t spl_fstypes = SPINLOCK_DEFAULT_INIT;
static struct VFS_FILESYSTEMS fstypes;

void
vfs_init_mount()
{
	memset(mountedfs, 0, sizeof(mountedfs));
//	DQUEUE_INIT(&fstypes);
}

static struct VFS_MOUNTED_FS*
vfs_get_availmountpoint()
{
	spinlock_lock(&spl_mountedfs);
	for (unsigned int n = 0; n < VFS_MOUNTED_FS_MAX; n++) {
		struct VFS_MOUNTED_FS* fs = &mountedfs[n];
		if ((fs->fs_flags & VFS_FLAG_INUSE) == 0) {
			fs->fs_flags |= VFS_FLAG_INUSE;
			spinlock_unlock(&spl_mountedfs);
			return fs;
		}
	}
	spinlock_unlock(&spl_mountedfs);
	return NULL;
}

errorcode_t
vfs_mount(const char* from, const char* to, const char* type, void* options)
{
	errorcode_t err;

	/* Attempt to locate the filesystem */
	struct VFS_FILESYSTEM_OPS* fsops = NULL;
	spinlock_lock(&spl_fstypes);
	if (!DQUEUE_EMPTY(&fstypes))
		DQUEUE_FOREACH(&fstypes, curfs, struct VFS_FILESYSTEM) {
			if (strcmp(curfs->fs_name, type) == 0) {
				/* Match */
				fsops = curfs->fs_fsops;
				break;
			}
		}
	spinlock_unlock(&spl_fstypes);
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
		vfs_deref_inode(inode);
	}

	struct DEVICE* dev = NULL;
	if (from != NULL) {
		dev = device_find(from);
		if (dev == NULL)
			return ANANAS_ERROR(NO_FILE);
	}

	fs->fs_device = dev;
	fs->fs_fsops = fsops;
	dcache_init(fs);

	err = fs->fs_fsops->mount(fs);
	if (err != ANANAS_ERROR_NONE) {
		dcache_destroy(fs);
		icache_destroy(fs);
		memset(fs, 0, sizeof(*fs));
		return err;
	}
	TRACE(VFS, INFO, "to='%s',fs=%p,rootinode=%p", to, fs, fs->fs_root_inode);

	KASSERT(fs->fs_root_inode != NULL, "successful mount without a root inode");
	KASSERT(S_ISDIR(fs->fs_root_inode->i_sb.st_mode), "root inode isn't a directory");
	/* The refcount must be two: one for the fs->root_inode reference and one for the cache */
	KASSERT(fs->fs_root_inode->i_refcount == 2, "bad refcount of root inode (must be 2, is %u)", fs->fs_root_inode->i_refcount);
	fs->fs_mountpoint = strdup(to);

	/*
	 * Override the previous inode with our root inode; this effectively hooks
	 * our filesystem to the parent.
	 */
	if (d != NULL) {
		if (d->d_entry_inode != NULL)
			vfs_deref_inode(d->d_entry_inode);
		d->d_entry_inode = fs->fs_root_inode;
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
		if ((fs->fs_flags & VFS_FLAG_INUSE) && (fs->fs_mountpoint != NULL) && (strcmp(fs->fs_mountpoint, path) == 0)) {
			/* Got it; disown it immediately. XXX What about pending inodes etc? */
			fs->fs_mountpoint = NULL;
			spinlock_unlock(&spl_mountedfs);
			/* XXX Ask filesystem politely to unmount */
			fs->fs_flags = 0; /* Available */
			return ANANAS_ERROR_OK;
		}
	}
	spinlock_unlock(&spl_mountedfs);
	return ANANAS_ERROR(BAD_HANDLE); /* XXX */
}

struct VFS_MOUNTED_FS*
vfs_get_rootfs()
{
	spinlock_lock(&spl_mountedfs);
	for (unsigned int n = 0; n < VFS_MOUNTED_FS_MAX; n++) {
		struct VFS_MOUNTED_FS* fs = &mountedfs[n];
		if ((fs->fs_flags & VFS_FLAG_INUSE) && (fs->fs_mountpoint != NULL) && (strcmp(fs->fs_mountpoint, "/") == 0)) {
			spinlock_unlock(&spl_mountedfs);
			return fs;
		}
	}
	spinlock_unlock(&spl_mountedfs);
	return NULL;
}

errorcode_t
vfs_register_filesystem(struct VFS_FILESYSTEM* fs)
{
	spinlock_lock(&spl_fstypes);
	/* Ensure the filesystem is not already registered */
	if (!DQUEUE_EMPTY(&fstypes))
		DQUEUE_FOREACH(&fstypes, curfs, struct VFS_FILESYSTEM) {
			if (strcmp(curfs->fs_name, fs->fs_name) != 0) {
				/* Duplicate filesystem type; refuse to register */
				spinlock_unlock(&spl_fstypes);
				return ANANAS_ERROR(FILE_EXISTS);
			}
		}

	/* Filesystem is clear; hook it up */
	DQUEUE_ADD_TAIL(&fstypes, fs);
	spinlock_unlock(&spl_fstypes);
	return ANANAS_ERROR_OK;
}

errorcode_t
vfs_unregister_filesystem(struct VFS_FILESYSTEM* fs)
{
	spinlock_lock(&spl_fstypes);
	DQUEUE_REMOVE(&fstypes, fs);
	spinlock_unlock(&spl_fstypes);
	return ANANAS_ERROR_OK;
}

#ifdef KDB
void
kdb_cmd_vfs_mounts(int num_args, char** arg)
{
	spinlock_lock(&spl_mountedfs);
	for (unsigned int n = 0; n < VFS_MOUNTED_FS_MAX; n++) {
		struct VFS_MOUNTED_FS* fs = &mountedfs[n];
		if ((fs->fs_flags & VFS_FLAG_INUSE) == 0)
			continue;
		kprintf(">> vfs=%p, flags=0x%x, mountpoint='%s'\n", fs, fs->fs_flags, fs->fs_mountpoint);
		icache_dump(fs);
		dcache_dump(fs);
	}
	spinlock_unlock(&spl_mountedfs);
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
