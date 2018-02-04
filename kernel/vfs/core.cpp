#include <ananas/types.h>
#include "kernel/bio.h"
#include "kernel/init.h"
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/types.h"
#include "kernel/vfs/mount.h"
#include "kernel/vfs/icache.h"

TRACE_SETUP;

static Result
vfs_init()
{
	vfs_init_mount();
	return Result::Success();
}

INIT_FUNCTION(vfs_init, SUBSYSTEM_VFS, ORDER_FIRST);

bool
vfs_is_filesystem_sane(struct VFS_MOUNTED_FS* fs)
{
	SpinlockGuard g(fs->fs_spinlock);
	return (fs->fs_flags & VFS_FLAG_ABANDONED) == 0;
}

Result
vfs_bget(struct VFS_MOUNTED_FS* fs, blocknr_t block, struct BIO** bio, int flags)
{
	if (!vfs_is_filesystem_sane(fs))
		return RESULT_MAKE_FAILURE(EIO);

	*bio = bio_get(fs->fs_device, block * (fs->fs_block_size / BIO_SECTOR_SIZE), fs->fs_block_size, flags);
	if (*bio == NULL)
		return RESULT_MAKE_FAILURE(EIO);
	return Result::Success();
}

size_t
vfs_filldirent(void** dirents, size_t* size, ino_t inum, const char* name, int namelen)
{
	/*
	 * First of all, ensure we have sufficient space. Note that we use the fact
	 * that de_name is only a single byte; we count it as the \0 byte.
	 */
	int de_length = sizeof(struct VFS_DIRENT) + namelen;
	if (*size < de_length)
		return 0;
	*size -= de_length;

	struct VFS_DIRENT* de = (struct VFS_DIRENT*)*dirents;
	*dirents = static_cast<void*>(static_cast<char*>(*dirents) + de_length);

	de->de_flags = 0; /* TODO */
	de->de_name_length = namelen;
	de->de_inum = inum;
	memcpy(de->de_name, name, namelen);
	de->de_name[namelen] = '\0'; /* zero terminate */

	/*
	 * TODO Now that we have the entry, this is a good time to add it to our
	 * dentry cache.
	 */
	return de_length;
}

void
vfs_set_inode_dirty(INode& inode)
{
	/* Ref the inode to prevent it from going away */
	vfs_ref_inode(inode);

	/* XXX For now, we just immediately write the inode */
	struct VFS_MOUNTED_FS* fs = inode.i_fs;
	fs->fs_fsops->write_inode(inode);

	vfs_deref_inode(inode);
}

/* vim:set ts=2 sw=2: */
