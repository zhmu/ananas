#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/icache.h"
#include "options.h"

TRACE_SETUP;

namespace Ananas {
namespace VFS {
namespace {
const size_t Max_Mounted_FS = 16;
} // unnamed namespace

Spinlock spl_mountedfs;
struct VFS_MOUNTED_FS mountedfs[Max_Mounted_FS];

Spinlock spl_fstypes;
VFSFileSystemList fstypes;

size_t GetMaxMountedFilesystems()
{
	return Max_Mounted_FS;
}

} // namespace VFS
} // namespace Ananas

void
vfs_init_mount()
{
	memset(Ananas::VFS::mountedfs, 0, sizeof(Ananas::VFS::mountedfs));
}

static struct VFS_MOUNTED_FS*
vfs_get_availmountpoint()
{
	SpinlockGuard g(Ananas::VFS::spl_mountedfs);
	for (unsigned int n = 0; n < Ananas::VFS::Max_Mounted_FS; n++) {
		struct VFS_MOUNTED_FS* fs = &Ananas::VFS::mountedfs[n];
		if ((fs->fs_flags & VFS_FLAG_INUSE) == 0) {
			fs->fs_flags |= VFS_FLAG_INUSE;
			return fs;
		}
	}
	return NULL;
}

errorcode_t
vfs_mount(const char* from, const char* to, const char* type, void* options)
{
	errorcode_t err;

	/*
	 * Attempt to locate the filesystem type; so we know what to call to mount
	 * it.
	 */
	struct VFS_FILESYSTEM_OPS* fsops = NULL;
	{
		SpinlockGuard g(Ananas::VFS::spl_fstypes);
		for(auto& curfs: Ananas::VFS::fstypes) {
			if (strcmp(curfs.fs_name, type) == 0) {
				/* Match */
				fsops = curfs.fs_fsops;
				break;
			}
		}
	}
	if (fsops == NULL)
		return ANANAS_ERROR(BAD_TYPE);

	/* Locate the device to mount from */
	Ananas::Device* dev = NULL;
	if (from != NULL) {
		dev = Ananas::DeviceManager::FindDevice(from);
		if (dev == NULL)
			return ANANAS_ERROR(NO_FILE);
	}

	/* Locate an available mountpoint and hook it up */
	struct VFS_MOUNTED_FS* fs = vfs_get_availmountpoint();
	if (fs == NULL)
		return ANANAS_ERROR(OUT_OF_HANDLES);
	fs->fs_device = dev;
	fs->fs_fsops = fsops;

	INode* root_inode = nullptr;
	err = fs->fs_fsops->mount(fs, root_inode);
	if (ananas_is_failure(err)) {
		memset(fs, 0, sizeof(*fs));
		return err;
	}
	TRACE(VFS, INFO, "to='%s',fs=%p,rootinode=%p", to, fs, root_inode);

	KASSERT(root_inode != NULL, "successful mount without a root inode");
	KASSERT(S_ISDIR(root_inode->i_sb.st_mode), "root inode isn't a directory");
	KASSERT(root_inode->i_refcount == 1, "bad refcount of root inode (must be 1, is %u)", root_inode->i_refcount);
	fs->fs_mountpoint = strdup(to);
	fs->fs_root_dentry = &dcache_create_root_dentry(fs);
	fs->fs_root_dentry->d_inode = root_inode; /* don't deref - we're giving the ref to the root dentry */

	/*	
	 * Now perform a root entry lookup; if this fails, we'll have to abort everything we have done before
	 * XXX Maybe special-case once we have a root filesystem?
	 */
	if (ananas_is_failure(err)) {
		memset(fs, 0, sizeof(*fs));
		return err;
	}

	/*
	 * Override the root path dentry with our root inode; this effectively hooks
	 * our filesystem to the parent. XXX I wonder if this is correct; we should
	 * always just hook our path to the fs root dentry... need to think about it
	 */
	DEntry* dentry_root = fs->fs_root_dentry;
	if (ananas_is_success(vfs_lookup(NULL, dentry_root, to)) &&
	    dentry_root != fs->fs_root_dentry) {
		if (dentry_root->d_inode != NULL)
			vfs_deref_inode(*dentry_root->d_inode);
		vfs_ref_inode(*root_inode);
		dentry_root->d_inode = root_inode;
	}

	return ananas_success();
}

void
vfs_abandon_device(Ananas::Device& device)
{
	SpinlockGuard g(Ananas::VFS::spl_mountedfs);

	for (unsigned int n = 0; n < Ananas::VFS::Max_Mounted_FS; n++) {
		struct VFS_MOUNTED_FS* fs = &Ananas::VFS::mountedfs[n];
		if ((fs->fs_flags & VFS_FLAG_INUSE) == 0 || (fs->fs_flags & VFS_FLAG_ABANDONED) || fs->fs_device != &device)
			continue;

		/*
		 * This filesystem can no longer operate sanely - ensure all requests for it are
		 * rejected instead of serviced.
		 *
		 * XXX We should at least start the unmount process here.
		 */
		 fs->fs_flags |= VFS_FLAG_ABANDONED;
		 fs->fs_device = nullptr;
	}
}

errorcode_t
vfs_unmount(const char* path)
{
	panic("fix me");

	SpinlockGuard g(Ananas::VFS::spl_mountedfs);

	for (unsigned int n = 0; n < Ananas::VFS::Max_Mounted_FS; n++) {
		struct VFS_MOUNTED_FS* fs = &Ananas::VFS::mountedfs[n];
		if ((fs->fs_flags & VFS_FLAG_INUSE) && (fs->fs_mountpoint != NULL) && (strcmp(fs->fs_mountpoint, path) == 0)) {
			/* Got it; disown it immediately. XXX What about pending inodes etc? */
			fs->fs_mountpoint = NULL;
			/* XXX Ask filesystem politely to unmount */
			fs->fs_flags = 0; /* Available */
			return ananas_success();
		}
	}
	return ANANAS_ERROR(BAD_HANDLE); /* XXX */
}

struct VFS_MOUNTED_FS*
vfs_get_rootfs()
{
	SpinlockGuard g(Ananas::VFS::spl_mountedfs);

	/* XXX the root fs should have a flag marking it as such */
	for (unsigned int n = 0; n < Ananas::VFS::Max_Mounted_FS; n++) {
		struct VFS_MOUNTED_FS* fs = &Ananas::VFS::mountedfs[n];
		if ((fs->fs_flags & VFS_FLAG_INUSE) && (fs->fs_mountpoint != NULL) && (strcmp(fs->fs_mountpoint, "/") == 0))
			return fs;
	}
	return NULL;
}

errorcode_t
vfs_register_filesystem(VFSFileSystem& fs)
{
	SpinlockGuard g(Ananas::VFS::spl_fstypes);

	/* Ensure the filesystem is not already registered */
	for(auto& curfs: Ananas::VFS::fstypes) {
		if (strcmp(curfs.fs_name, fs.fs_name) == 0) {
			/* Duplicate filesystem type; refuse to register */
			return ANANAS_ERROR(FILE_EXISTS);
		}
	}

	/* Filesystem is clear; hook it up */
	Ananas::VFS::fstypes.push_back(fs);
	return ananas_success();
}

errorcode_t
vfs_unregister_filesystem(VFSFileSystem& fs)
{
	SpinlockGuard g(Ananas::VFS::spl_fstypes);

	Ananas::VFS::fstypes.remove(fs);
	return ananas_success();
}

#ifdef OPTION_KDB
KDB_COMMAND(mounts, NULL, "Shows current mounts")
{
	SpinlockGuard g(Ananas::VFS::spl_fstypes);

	for (unsigned int n = 0; n < Ananas::VFS::Max_Mounted_FS; n++) {
		struct VFS_MOUNTED_FS* fs = &Ananas::VFS::mountedfs[n];
		if ((fs->fs_flags & VFS_FLAG_INUSE) == 0)
			continue;
		kprintf(">> vfs=%p, flags=0x%x, mountpoint='%s'\n", fs, fs->fs_flags, fs->fs_mountpoint);
	}
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
