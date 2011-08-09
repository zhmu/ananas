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

#define INODE_ASSERT_SANE(i) \
	KASSERT(((i)->i_flags & INODE_FLAG_GONE) == 0, "referencing gone inode"); \
	KASSERT((i)->i_refcount > 0, "referencing inode with no refs");

static errorcode_t
vfs_init()
{
	vfs_init_mount();
	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(vfs_init, SUBSYSTEM_VFS, ORDER_FIRST);

struct VFS_INODE*
vfs_make_inode(struct VFS_MOUNTED_FS* fs, const void* fsop)
{
	struct VFS_INODE* inode = kmalloc(sizeof(struct VFS_INODE) + fs->fs_fsop_size);

	/* Set up the basic inode information */
	memset(inode, 0, sizeof(*inode));
	mutex_init(&inode->i_mutex, "inode");
	inode->i_refcount = 1;
	inode->i_fs = fs;
	memcpy(inode->i_fsop, fsop, fs->fs_fsop_size);
	/* Fill out the stat fields we can */
	inode->i_sb.st_dev = (dev_t)(uintptr_t)fs->fs_device;
	inode->i_sb.st_rdev = (dev_t)(uintptr_t)fs->fs_device;
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
	KASSERT(inode->i_flags & INODE_FLAG_GONE, "destroying inode that isn't gone");
	KASSERT(inode->i_refcount == 0, "destroying inode which still has refs");
	kfree(inode);
	TRACE(VFS, INFO, "destroyed inode=%p", inode);
}

void icache_ensure_inode_gone(struct VFS_INODE* inode);

void
vfs_deref_inode_locked(struct VFS_INODE* inode)
{
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->i_refcount);
	if (--inode->i_refcount > 0) {
		/* Refcount isn't zero; don't throw the inode away */
		INODE_UNLOCK(inode);
		return;
	}

	/*
	 * Note: we never unlock - the inode does not exist past here and all references should
	 * be gone. If this isn't the case, that's a bug - which we hope to catch by setting
	 * the GONE flag so people can assert on it.
	 */
	inode->i_flags |= INODE_FLAG_GONE;
icache_ensure_inode_gone(inode);
	if (inode->i_fs->fs_fsops->alloc_inode != NULL) {
		inode->i_fs->fs_fsops->destroy_inode(inode);
	} else {
		vfs_destroy_inode(inode);
	}
}

void
vfs_deref_inode(struct VFS_INODE* inode)
{
	INODE_ASSERT_SANE(inode);
	INODE_LOCK(inode);
	vfs_deref_inode_locked(inode);
	/* INODE_UNLOCK(inode); - no need, inode must be dead by now */
}

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
vfs_ref_inode(struct VFS_INODE* inode)
{
	KASSERT(inode != NULL, "reffing a null inode");
	TRACE(VFS, FUNC, "inode=%p,cur refcount=%u", inode, inode->i_refcount);
	INODE_ASSERT_SANE(inode);

	INODE_LOCK(inode);
	KASSERT(inode->i_refcount > 0, "referencing a dead inode");
	inode->i_refcount++;
	INODE_UNLOCK(inode);
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
