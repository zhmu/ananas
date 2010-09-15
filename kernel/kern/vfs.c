#include <sys/types.h>
#include <sys/bio.h>
#include <sys/device.h>
#include <sys/lib.h>
#include <sys/mm.h>
#include <sys/vfs.h>

#define VFS_MOUNTED_FS_MAX	16

struct VFS_MOUNTED_FS mountedfs[VFS_MOUNTED_FS_MAX];

extern struct VFS_FILESYSTEM_OPS fsops_ext2;
extern struct VFS_FILESYSTEM_OPS fsops_iso9660;

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

int
vfs_mount(const char* from, const char* to, const char* type, void* options)
{
	struct VFS_MOUNTED_FS* fs = vfs_get_availmountpoint();
	if (fs == NULL)
		return -1;

	struct DEVICE* dev = device_find(from);
	if (dev == NULL)
		return -2;

	fs->device = dev;
	fs->mountpoint = strdup(to);

	/* XXX kludge */
	fs->fsops = &fsops_ext2;
	//fs->fsops = &fsops_iso9660;

	if (!fs->fsops->mount(fs)) {
		kfree((void*)fs->mountpoint);
		memset(fs, 0, sizeof(*fs));
		return -3;
	}
	
	return 1;
}

struct VFS_INODE*
vfs_make_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode = kmalloc(sizeof(struct VFS_INODE));

	memset(inode, 0, sizeof(*inode));
	inode->fs = fs;
	return inode;
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

struct VFS_INODE*
vfs_get_inode(struct VFS_MOUNTED_FS* fs, void* fsop)
{
	struct VFS_INODE* inode = vfs_alloc_inode(fs);
	if (inode == NULL)
		return NULL;
	if (!fs->fsops->read_inode(inode, fsop)) {
		vfs_free_inode(inode);
		return NULL;
	}
	return inode;
}

struct VFS_INODE*
vfs_lookup(const char* dentry)
{
	char tmp[VFS_MAX_NAME_LEN + 1];

	/* XXX we only consider the first filesystem! */
	struct VFS_MOUNTED_FS* fs = &mountedfs[0];
	struct VFS_INODE* rootinode = fs->root_inode;
	if (*dentry == '/')
		dentry++;

	const char* curdentry = dentry;
	const char* lookupptr;
	while (1) {
		char* ptr = strchr(curdentry, '/');
		if (ptr != NULL) {
			/* There's a slash in the path - must lookup next part */
			strncpy(tmp, curdentry, ptr - curdentry);
			curdentry = ++ptr;
			lookupptr = tmp;
		} else {
			lookupptr = curdentry;
			curdentry = NULL;
		}

		/* Attempt to look up whatever entry we need */
		struct VFS_INODE* inode = rootinode->iops->lookup(rootinode, lookupptr);
		if (inode == NULL) {
			if (rootinode != fs->root_inode) vfs_free_inode(rootinode);
			return NULL;
		}

		/*
		 * OK, the lookup succeeded. Continue if needed.
		 */
		if (curdentry == NULL) {
			if (rootinode != fs->root_inode) vfs_free_inode(rootinode);
			return inode;
		}

		rootinode = inode;
	}

	/* NOTREACHED */
}

int
vfs_open(const char* fname, struct VFS_FILE* file)
{
	struct VFS_INODE* inode = vfs_lookup(fname);
	if (inode == NULL)
		return 0;

	memset(file, 0, sizeof(struct VFS_FILE));
	file->inode = inode;
	file->offset = 0;
	return 1;
}

void
vfs_close(struct VFS_FILE* file)
{
	if(file->inode != NULL)
		vfs_free_inode(file->inode);
	file->inode = NULL; file->device = NULL;
}

size_t
vfs_read(struct VFS_FILE* file, void* buf, size_t len)
{
	KASSERT(file->inode != NULL || file->device != NULL, "vfs_read on nonbacked file");
	if (file->device != NULL) {
		if (file->device->driver == NULL || file->device->driver->drv_read == NULL)
			return -1;
		else {
			return file->device->driver->drv_read(file->device, buf, len, 0);
		}
	}

	if (file->inode == NULL || file->inode->iops == NULL || file->inode->iops->read == NULL)
		return -1;
	return file->inode->iops->read(file, buf, len);
}

size_t
vfs_write(struct VFS_FILE* file, const void* buf, size_t len)
{
	KASSERT(file->inode != NULL || file->device != NULL, "vfs_write on nonbacked file");
	if (file->device != NULL)
		if (file->device->driver == NULL || file->device->driver->drv_write == NULL)
			return -1;
		else
			return file->device->driver->drv_write(file->device, buf, len, 0);

	if (file->inode == NULL || file->inode->iops == NULL || file->inode->iops->write != NULL)
		return -1;
	return file->inode->iops->write(file, buf, len);
}

int
vfs_seek(struct VFS_FILE* file, off_t offset)
{
	if (file->inode == NULL)
		return 0;
	if (offset > file->inode->length)
		return 0;
	file->offset = offset;
	return 1;
}

size_t
vfs_readdir(struct VFS_FILE* file, struct VFS_DIRENT* dirents, size_t numents)
{
	KASSERT(file->inode != NULL, "vfs_readdir on nonbacked file");
	/* XXX check dir */

	return file->inode->iops->readdir(file, dirents, numents);
}	

int
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
