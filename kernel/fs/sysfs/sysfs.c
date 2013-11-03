#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/error.h>
#include <ananas/vfs.h>
#include <ananas/vfs/generic.h>
#include <ananas/vfs/mount.h>
#include <ananas/init.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include "entry.h"

TRACE_SETUP;

#define SYSFS_BLOCK_SIZE 512

struct SYSFS_INODE_PRIVDATA {
	struct SYSFS_ENTRY* p_entry;
};

static struct VFS_INODE*
sysfs_alloc_inode(struct VFS_MOUNTED_FS* fs, const void* fsop)
{
	struct VFS_INODE* inode = vfs_make_inode(fs, fsop);
	if (inode == NULL)
		return NULL;
	inode->i_privdata = kmalloc(sizeof(struct SYSFS_INODE_PRIVDATA));
	memset(inode->i_privdata, 0, sizeof(struct SYSFS_INODE_PRIVDATA));
	return inode;
}

static void
sysfs_destroy_inode(struct VFS_INODE* inode)
{
	kfree(inode->i_privdata);
	vfs_destroy_inode(inode);
}

static errorcode_t
sysfs_readdir(struct VFS_FILE* file, void* dirents, size_t* len)
{
	struct SYSFS_INODE_PRIVDATA* privdata = file->f_inode->i_privdata;
	struct SYSFS_ENTRY* entry = privdata->p_entry;
	size_t left = *len, written = 0;

	/* Start at our current node's children */
	entry = entry->e_children;

	/*
	 * We use file->f_offset as the entry where to start; first of all, go to the
	 * first entry we have to manage.
	 */
	while (file->f_offset > 0 && entry != NULL) {
		entry = entry->e_next;
		file->f_offset--;
	}

	while (left > 0 && entry != NULL) {
		int filled = vfs_filldirent(&dirents, &left, (const void*)&entry->e_inum, file->f_inode->i_fs->fs_fsop_size, entry->e_name, strlen(entry->e_name));
		if (!filled) {
			/* out of space! */
			break;
		}
		written += filled;
		entry = entry->e_next;
		file->f_offset++;
	}

	*len = written;
	return ANANAS_ERROR_OK;
}

static struct VFS_INODE_OPS sysfs_dir_ops = {
	.readdir = sysfs_readdir,
	.lookup = vfs_generic_lookup
};

static errorcode_t
sysfs_read(struct VFS_FILE* file, void* buf, size_t* len)
{
	struct SYSFS_INODE_PRIVDATA* privdata = file->f_inode->i_privdata;
	struct SYSFS_ENTRY* entry = privdata->p_entry;

	*len = 0;
	if (entry->e_read_fn == NULL)
		return ANANAS_ERROR(BAD_OPERATION);

	/*
	 * Our read function works by supplying some temporary storage which it can use
	 * at will; upon success, we assume it will set 'start' to some address within
	 * the buffer.
	 */
	char* buffer = kmalloc(1);
	char* start = buffer;
	errorcode_t err = entry->e_read_fn(entry, buffer, &start, file->f_offset, len);
	if (err == ANANAS_ERROR_OK) {
		/* Read went OK; update content */
		KASSERT(start >= buffer && start < buffer + PAGE_SIZE, "start offset out of range");
		memcpy(buf, start, *len);
		file->f_offset += *len;
	}
	kfree(buffer);
	return err;
}

static errorcode_t
sysfs_write(struct VFS_FILE* file, const void* buf, size_t* len)
{
	struct SYSFS_INODE_PRIVDATA* privdata = file->f_inode->i_privdata;
	struct SYSFS_ENTRY* entry = privdata->p_entry;

	*len = 0;
	if (entry->e_write_fn == NULL)
		return ANANAS_ERROR(BAD_OPERATION);

	kprintf("sysfs_write()\n");
	errorcode_t err = ANANAS_ERROR_OK;
	err = ANANAS_ERROR(IO);
	return err;
}

static struct VFS_INODE_OPS sysfs_file_ops = {
	.read = sysfs_read,
	.write = sysfs_write,
};

static errorcode_t
sysfs_read_inode(struct VFS_INODE* inode, void* fsop)
{
	struct SYSFS_ENTRY* entry = sysfs_find_entry(fsop);
	if (entry == NULL)
			return ANANAS_ERROR(NO_FILE);

	struct SYSFS_INODE_PRIVDATA* privdata = inode->i_privdata;
	privdata->p_entry = entry;
	inode->i_sb.st_ino = entry->e_inum;
	inode->i_sb.st_mode = entry->e_mode;
	inode->i_sb.st_nlink = 1;
	inode->i_sb.st_uid = 0;
	inode->i_sb.st_gid = 0;
	inode->i_sb.st_atime = 0;
	inode->i_sb.st_mtime = 0;
	inode->i_sb.st_ctime = 0;
	inode->i_sb.st_size = 0;
	if (S_ISDIR(entry->e_mode)) {
		inode->i_iops = &sysfs_dir_ops;
	} else {
		inode->i_iops = &sysfs_file_ops;
	}
	return ANANAS_ERROR_NONE;
}

static errorcode_t
sysfs_mount(struct VFS_MOUNTED_FS* fs)
{
	fs->fs_block_size = SYSFS_BLOCK_SIZE;
	fs->fs_fsop_size = sizeof(uint32_t);
	icache_init(fs);
	uint32_t root_fsop = SYSFS_ROOTINODE_NUM;
	fs->fs_root_inode = sysfs_alloc_inode(fs, (const void*)&root_fsop);
	return vfs_get_inode(fs, &root_fsop, &fs->fs_root_inode);
}

static struct VFS_FILESYSTEM_OPS fsops_sysfs = {
	.mount = sysfs_mount,
	.alloc_inode = sysfs_alloc_inode,
	.destroy_inode = sysfs_destroy_inode,
	.read_inode = sysfs_read_inode
};

static struct VFS_FILESYSTEM fs_sysfs = {
	.fs_name = "sysfs",
	.fs_fsops = &fsops_sysfs
};

/*
 * Initialization is in two steps; we can immediately make our administration
 * work. Once the VFS is ready enough, we'll hook ourselves to it as well.
 */
static errorcode_t
sysfs_preinit()
{
	sysfs_init_structs();
	return ANANAS_ERROR_OK;
}

errorcode_t
sysfs_init()
{
	return vfs_register_filesystem(&fs_sysfs);
}

static errorcode_t
sysfs_exit()
{
	return vfs_unregister_filesystem(&fs_sysfs);
}

INIT_FUNCTION(sysfs_preinit, SUBSYSTEM_HANDLE, ORDER_LAST);
INIT_FUNCTION(sysfs_init, SUBSYSTEM_VFS, ORDER_MIDDLE);
EXIT_FUNCTION(sysfs_exit);

/* vim:set ts=2 sw=2: */
