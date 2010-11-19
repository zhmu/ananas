#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>
#include "options.h"

TRACE_SETUP;

#define VFS_MOUNTED_FS_MAX	16

struct VFS_MOUNTED_FS mountedfs[VFS_MOUNTED_FS_MAX];

void
vfs_init()
{
	memset(mountedfs, 0, sizeof(mountedfs));
}

static struct VFS_MOUNTED_FS*
vfs_get_availmountpoint()
{
	for (unsigned int n = 0; n < VFS_MOUNTED_FS_MAX; n++) {
		struct VFS_MOUNTED_FS* fs = &mountedfs[n];
		/* XXX lock etc */
		if (fs->mountpoint == NULL)
			return fs;
	}
	return NULL;
}

static struct VFS_MOUNTED_FS*
vfs_get_rootfs()
{
	for (unsigned int n = 0; n < VFS_MOUNTED_FS_MAX; n++) {
		struct VFS_MOUNTED_FS* fs = &mountedfs[n];
		/* XXX lock etc */
		if (fs->mountpoint != NULL && strcmp(fs->mountpoint, "/") == 0)
			return fs;
	}
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
	return NULL;
}

errorcode_t
vfs_mount(const char* from, const char* to, const char* type, void* options)
{
	struct VFS_FILESYSTEM_OPS* fsops = vfs_get_fstype(type);
	if (fsops == NULL)
		return ANANAS_ERROR(BAD_TYPE);
	struct VFS_MOUNTED_FS* fs = vfs_get_availmountpoint();
	if (fs == NULL)
		return ANANAS_ERROR(OUT_OF_HANDLES);

	struct DEVICE* dev = device_find(from);
	if (dev == NULL)
		return ANANAS_ERROR(NO_FILE);

	fs->device = dev;
	fs->mountpoint = strdup(to);
	fs->fsops = fsops;

	int ret = fs->fsops->mount(fs);
	if (ret != ANANAS_ERROR_NONE) {
		kfree((void*)fs->mountpoint);
		memset(fs, 0, sizeof(*fs));
	}
	
	return ret;
}

struct VFS_INODE*
vfs_make_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode = kmalloc(sizeof(struct VFS_INODE));

	memset(inode, 0, sizeof(*inode));
	inode->fs = fs;
	/* Fill out the stat fields we can */
	inode->sb.st_dev = (dev_t)fs->device;
	inode->sb.st_rdev = (dev_t)fs->device;
	inode->sb.st_blksize = fs->block_size;
	return inode;
}

void
vfs_dump_inode(struct VFS_INODE* inode)
{
	struct stat* sb = &inode->sb;
	kprintf("ino     = %u\n", sb->st_ino);
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

struct VFS_INODE*
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
vfs_free_inode(struct VFS_INODE* inode)
{
	if (inode->fs->fsops->alloc_inode != NULL) {
		inode->fs->fsops->free_inode(inode);
	} else {
		vfs_destroy_inode(inode);
	}
}

struct BIO*
vfs_bread(struct VFS_MOUNTED_FS* fs, block_t block, size_t len)
{
	return bio_read(fs->device, block, len);
}

errorcode_t
vfs_get_inode(struct VFS_MOUNTED_FS* fs, void* fsop, struct VFS_INODE** destinode)
{
	struct VFS_INODE* inode = vfs_alloc_inode(fs);
	if (inode == NULL)
		return ANANAS_ERROR(OUT_OF_HANDLES);
	errorcode_t result = fs->fsops->read_inode(inode, fsop);
	if (result != ANANAS_ERROR_NONE) {
		vfs_free_inode(inode);
		return result;
	}
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

	struct VFS_INODE* lookup_root_inode = curinode;
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

		/* If the entry to find is '.', we are done */
		if (strcmp(curlookup, ".") == 0)
			break;

		/*
		 * We need to recurse; this can only be done if this is a directory, so
		 * refuse if this isn't the case.
		 */
		if (S_ISDIR(curinode->sb.st_mode) == 0) {
			if (lookup_root_inode != curinode)
				vfs_free_inode(curinode);
			return ANANAS_ERROR(NO_FILE);
		}

		/*
	 	 * Attempt to look up whatever entry we need. Once this is done, we can
		 * get rid of the current loookup inode (as it won't be the inode itself
		 * since we special-case "." above).
		 */
		struct VFS_INODE* inode;
		int err = curinode->iops->lookup(curinode, &inode, curlookup);
		if (lookup_root_inode != curinode)
			vfs_free_inode(curinode);
		ANANAS_ERROR_RETURN(err);

		curinode = inode;
	}
	*destinode = curinode;
	return ANANAS_ERROR_OK;
}

errorcode_t
vfs_open(const char* fname, struct VFS_INODE* cwd, struct VFS_FILE* file)
{
	struct VFS_INODE* inode;
	int err = vfs_lookup(cwd, &inode, fname);
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
		vfs_free_inode(file->inode);
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

/* vim:set ts=2 sw=2: */
