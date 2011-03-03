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
vfs_init_mount()
{
	memset(mountedfs, 0, sizeof(mountedfs));
	spinlock_init(&spl_mountedfs);
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
