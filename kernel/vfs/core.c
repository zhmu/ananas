#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/vfs/types.h>
#include <ananas/vfs/mount.h>

TRACE_SETUP;

static errorcode_t
vfs_init()
{
	vfs_init_mount();
	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(vfs_init, SUBSYSTEM_VFS, ORDER_FIRST);

errorcode_t
vfs_bget(struct VFS_MOUNTED_FS* fs, blocknr_t block, struct BIO** bio, int flags)
{
	*bio = bio_get(fs->fs_device, block * (fs->fs_block_size / BIO_SECTOR_SIZE), fs->fs_block_size, flags);
	if (*bio == NULL)
		return ANANAS_ERROR(IO);
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

	/*
	 * TODO Now that we have the entry, this is a good time to add it to our
	 * dentry cache.
	 */
	return de_length;
}

void
vfs_set_inode_dirty(struct VFS_INODE* inode)
{
	/* Ref the inode to prevent it from going away */
	vfs_ref_inode(inode);
	
	/* XXX For now, we just immediately write the inode */
	struct VFS_MOUNTED_FS* fs = inode->i_fs;
	fs->fs_fsops->write_inode(inode);

	vfs_deref_inode(inode);
}

/* vim:set ts=2 sw=2: */
