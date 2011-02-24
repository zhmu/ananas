#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/schedule.h>
#include <ananas/trace.h>
#include <ananas/vfs/mount.h>
#include "options.h"

TRACE_SETUP;

void
vfs_init()
{
	vfs_init_mount();
}

struct VFS_INODE*
vfs_make_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode = kmalloc(sizeof(struct VFS_INODE));

	/* Set up the basic inode information */
	memset(inode, 0, sizeof(*inode));
	spinlock_init(&inode->i_spinlock);
	inode->i_refcount++;
	inode->i_fs = fs;
	/* Fill out the stat fields we can */
	inode->i_sb.st_dev = (dev_t)fs->fs_device;
	inode->i_sb.st_rdev = (dev_t)fs->fs_device;
	inode->i_sb.st_blksize = fs->fs_block_size;
	TRACE(VFS, INFO, "made inode inode=%p with refcount=%u", inode, inode->i_refcount);
	return inode;
}

void
vfs_dump_inode(struct VFS_INODE* inode)
{
	struct stat* sb = &inode->i_sb;
	kprintf("ino     = %u\n", sb->st_ino);
	kprintf("refcount= %u\n", inode->i_refcount);
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

	if (fs->fs_fsops->alloc_inode != NULL) {
		inode = fs->fs_fsops->alloc_inode(fs);
	} else {
		inode = vfs_make_inode(fs);
	}
	return inode;
}

void
vfs_deref_inode_locked(struct VFS_INODE* inode)
{
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->i_refcount);
	if (--inode->i_refcount > 0) {
		/* Refcount isn't zero; don't throw the inode away */
		INODE_UNLOCK(inode);
		return;
	}

	/* Note: we never unlock - the inode does not exist past here */
	if (inode->i_fs->fs_fsops->alloc_inode != NULL) {
		inode->i_fs->fs_fsops->destroy_inode(inode);
	} else {
		vfs_destroy_inode(inode);
	}
}

void
vfs_deref_inode(struct VFS_INODE* inode)
{
	INODE_LOCK(inode);
	vfs_deref_inode_locked(inode);
	INODE_UNLOCK(inode);
}

struct BIO*
vfs_bread(struct VFS_MOUNTED_FS* fs, block_t block, size_t len)
{
	return bio_read(fs->fs_device, block, len);
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
	errorcode_t result = fs->fs_fsops->read_inode(inode, fsop);
	if (result != ANANAS_ERROR_NONE) {
		vfs_deref_inode(inode);
		return result;
	}
	icache_set_pending(fs, ii, inode);
	ii->inode = inode;
	*destinode = inode;
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
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->i_refcount);
	INODE_LOCK(inode);
	KASSERT(inode->i_refcount > 0, "referencing a dead inode");
	inode->i_refcount++;
	INODE_UNLOCK(inode);
}

/* vim:set ts=2 sw=2: */
