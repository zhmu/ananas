/*
 * Ananas devfs driver.
 *
 * This filesystem is constructed, i.e. it does not have a backing device but
 * will rather generate all data on the fly. It will take the device tree and
 * make it accessible to third party programs.
 */
#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/error.h>
#include <ananas/vfs.h>
#include <ananas/vfs/generic.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/mm.h>

TRACE_SETUP;

#define DEVFS_BLOCK_SIZE 512
#define DEVFS_ROOTINODE_FSOP 1

struct DEVFS_INODE_PRIVDATA {
	device_t device;
};

static struct VFS_INODE*
devfs_alloc_inode(struct VFS_MOUNTED_FS* fs)
{
	struct VFS_INODE* inode = vfs_make_inode(fs);
	if (inode == NULL)
		return NULL;
	inode->i_privdata = kmalloc(sizeof(struct DEVFS_INODE_PRIVDATA));
	memset(inode->i_privdata, 0, sizeof(struct DEVFS_INODE_PRIVDATA));
	return inode;
}

static void
devfs_destroy_inode(struct VFS_INODE* inode)
{
	kfree(inode->i_privdata);
	vfs_destroy_inode(inode);
}

static errorcode_t
devfs_readdir_root(struct VFS_FILE* file, void* dirents, size_t* len)
{
	size_t left = *len, written = 0;
	uint32_t inum = DEVFS_ROOTINODE_FSOP + 1;

	struct DEVICE* dev = DQUEUE_HEAD(device_get_queue());
	for (unsigned int i = 0; i < file->offset && dev != NULL; i++) {
		dev = DQUEUE_NEXT(dev); inum++;
	}

	while (left > 0 && dev != NULL) {
		char devname[128 /* XXX */];
		sprintf(devname, "%s%u", dev->name, dev->unit);
		int filled = vfs_filldirent(&dirents, &left, (const void*)&inum, file->inode->i_fs->fsop_size, devname, strlen(devname));
		if (!filled) {
			/* out of space! */
			break;
		}
		written += filled; inum++;
		file->offset++; dev = DQUEUE_NEXT(dev);
	}

	*len = written;
	return ANANAS_ERROR_OK;
}

static struct VFS_INODE_OPS devfs_rootdir_ops = {
	.readdir = devfs_readdir_root,
	.lookup = vfs_generic_lookup
};

static errorcode_t
devfs_read(struct VFS_FILE* file, void* buf, size_t* len)
{
	struct DEVFS_INODE_PRIVDATA* privdata = file->inode->i_privdata;
	errorcode_t err = ANANAS_ERROR_OK;

	if (privdata->device->driver->drv_read != NULL) {
		size_t length = *len;
		err = device_read(privdata->device, buf, &length, file->offset);
		if (err == ANANAS_ERROR_NONE) {
			/* Read went OK; update the file pointer */
			file->offset += length;
			*len = length;
		}
	} else if (privdata->device->driver->drv_bread != NULL) {
		/*
		 * This is a block device; I/O must be done using BIO calls.
		 */
		off_t offset = file->offset / 512; /* XXX */
		struct BIO* bio = bio_read(privdata->device, offset, *len);
		if (BIO_IS_ERROR(bio)) {
			err = ANANAS_ERROR(IO);
		} else {
			memcpy(buf, BIO_DATA(bio), bio->length);
			file->offset += (bio->length / 512); /* XXX */
			*len = bio->length;
		}
		bio_free(bio);
	} else {
		err = ANANAS_ERROR(BAD_OPERATION);
	}
	return err;
}

static errorcode_t
devfs_write(struct VFS_FILE* file, const void* buf, size_t* len)
{
	kprintf("devfs_write: todo\n");
	return ANANAS_ERROR(IO);
}

static struct VFS_INODE_OPS devfs_file_ops = {
	.read = devfs_read,
	.write = devfs_write,
};

static errorcode_t
devfs_read_inode(struct VFS_INODE* inode, void* fsop)
{
	struct DEVFS_INODE_PRIVDATA* privdata = inode->i_privdata;
	uint32_t ino = *(uint32_t*)fsop;

	inode->i_sb.st_ino = ino;
	inode->i_sb.st_mode = 0444;
	inode->i_sb.st_nlink = 1;
	inode->i_sb.st_uid = 0;
	inode->i_sb.st_gid = 0;
	inode->i_sb.st_atime = 0;
	inode->i_sb.st_mtime = 0;
	inode->i_sb.st_ctime = 0;
	inode->i_sb.st_size = 0;

	if (ino == DEVFS_ROOTINODE_FSOP) {
		inode->i_sb.st_mode |= S_IFDIR | 0111;
		inode->i_iops = &devfs_rootdir_ops;
	} else {
		inode->i_iops = &devfs_file_ops;

		/* Obtain the device pointer XXX This is racey */
		struct DEVICE* dev = DQUEUE_HEAD(device_get_queue());
		for (unsigned int i = DEVFS_ROOTINODE_FSOP + 1; i < ino && dev != NULL; i++) {
			dev = DQUEUE_NEXT(dev);
		}
		if (dev == NULL)
			return ANANAS_ERROR(NO_FILE);

		/* Advertise a character, block or regular file based on available driver functions */
		if (dev->driver->drv_read != NULL || dev->driver->drv_write != NULL)
			inode->i_sb.st_mode |= S_IFCHR;
		else if (dev->driver->drv_bread != NULL || dev->driver->drv_bwrite != NULL)
			inode->i_sb.st_mode |= S_IFBLK;
		else
			inode->i_sb.st_mode |= S_IFREG;
		privdata->device = dev;
	}
	return ANANAS_ERROR_NONE;
}

static errorcode_t
devfs_mount(struct VFS_MOUNTED_FS* fs)
{
	fs->block_size = DEVFS_BLOCK_SIZE;
	fs->fsop_size = sizeof(uint32_t);
	fs->root_inode = devfs_alloc_inode(fs);
	uint32_t root_fsop = DEVFS_ROOTINODE_FSOP;
	return vfs_get_inode(fs, &root_fsop, &fs->root_inode);
}

struct VFS_FILESYSTEM_OPS fsops_devfs = {
	.mount = devfs_mount,
	.alloc_inode = devfs_alloc_inode,
	.destroy_inode = devfs_destroy_inode,
	.read_inode = devfs_read_inode
};

/* vim:set ts=2 sw=2: */
