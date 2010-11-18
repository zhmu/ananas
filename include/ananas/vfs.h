#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/stat.h>

#ifndef __SYS_VFS_H__
#define __SYS_VFS_H__

#define VFS_MAX_NAME_LEN	255

struct VFS_MOUNTED_FS;
struct VFS_INODE_OPS;
struct VFS_FILESYSTEM_OPS;

/*
 * VFS_INODE refers to an inode.
 */
struct VFS_INODE {
	struct stat sb;				/* Inode information */
	struct VFS_INODE_OPS* iops;		/* Inode operations */

	struct VFS_MOUNTED_FS* fs;		/* Filesystem where the inode lives */
	void*		privdata;		/* Filesystem-specific data */
};

/*
 * VFS_FILE refers to an opened file. It will have an associated
 * inode and a position.
 */
struct VFS_FILE {
	off_t			offset;
	/*
	 * An opened file can have an inode or device as backend.
	 */
	struct VFS_INODE*	inode;
	struct DEVICE*		device;
};

/*
 * Directory entry, as returned by the kernel. The fsop length is stored
 * for easy iteration on the caller side.
 */
struct VFS_DIRENT {
	uint32_t	de_flags;		/* Flags */
	uint8_t		de_fsop_length;		/* Length of identifier */
	uint8_t		de_name_length;		/* Length of name */
	char		de_fsop[1];		/* Identifier */
	/*
	 * de_name will be stored directly after the fsop.
	 */
};
#define DE_LENGTH(x) (sizeof(struct VFS_DIRENT) + (x)->de_fsop_length + (x)->de_name_length)
#define DE_FSOP(x) (&((x)->de_fsop))
#define DE_NAME(x) (&((x)->de_fsop[(x)->de_fsop_length]))


/*
 * VFS_MOUNTED_FS is used to refer to a mounted filesystem. Note that
 * we will also use it during the filesystem-operations mount() call.
 */
struct VFS_MOUNTED_FS {
	device_t	device;			/* Device where the filesystem lives */
	const char*	mountpoint;		/* Mount point */
	uint32_t	block_size;		/* Block size */
	uint8_t		fsop_size;		/* FSOP identifier length */
	void*		privdata;		/* Private filesystem data */
 
	struct VFS_FILESYSTEM_OPS* fsops;	/* Filesystem operations */
	struct VFS_INODE* root_inode;		/* Filesystem's root inode */
};

/*
 * VFS_FILESYSTEM_OPS are operations corresponding to the highest level:
 * mount/unmount a filesystem and obtaining statistics.
 */
struct VFS_FILESYSTEM_OPS {
	/*
	 * Mount a filesystem. device, mountpoint and fsops in 'fs' are
	 * guaranteed to be filled out.
	 */
	errorcode_t (*mount)(struct VFS_MOUNTED_FS* fs);

	/*
	 * Allocate an inode. The purpose for this function is to initialize
	 * the 'privdata' field of the inode - the function should call
	 * vfs_make_inode() to obtain the new inode.
	 */
	struct VFS_INODE* (*alloc_inode)(struct VFS_MOUNTED_FS* fs);

	/*
	 * Free an inode. The purpose for this function is to deinitialize
	 * the 'privdata' field of the inode. The function should end by
	 * calling vfs_destroy_inode() to remove the inode.
	 */
	void (*free_inode)(struct VFS_INODE* inode);

	/*
	 * Read an inode from disk; inode is pre-allocated using alloc_inode().
	 * The 'fs' field of the inode is guaranteed to be filled out.
	 */
	errorcode_t (*read_inode)(struct VFS_INODE* inode, void* fsop);
};

struct VFS_INODE_OPS {
	/*
	 * Reads directory entries. Must set length to amount of data filled on
	 * success.
	 */
	errorcode_t (*readdir)(struct VFS_FILE* file, void* buf, size_t* length);

	/*
	 * Looks up an entry within a directory, updates 'destinode' on success.
	 */
	errorcode_t (*lookup)(struct VFS_INODE* dirinode, struct VFS_INODE** destinode, const char* dentry);

	/*
	 * Reads inode data to a buffer, up to len bytes. Must update len on success
	 * with the amount of data read.
	 */
	errorcode_t (*read)(struct VFS_FILE* file, void* buf, size_t* len);

	/*
	 * Writes inode data from a buffer, up to len bytes. Must update len on success
	 * wit hthe amount of data written.
	 */
	errorcode_t (*write)(struct VFS_FILE* file, const void* buf, size_t* len);
};

void vfs_init();
errorcode_t vfs_mount(const char* from, const char* to, const char* type, void* options);
struct BIO* vfs_bread(struct VFS_MOUNTED_FS* fs, block_t block, size_t len);

/* Low-level interface */
struct VFS_INODE* vfs_alloc_inode(struct VFS_MOUNTED_FS* fs);
struct VFS_INODE* vfs_make_inode(struct VFS_MOUNTED_FS* fs);
void vfs_free_inode(struct VFS_INODE* inode);
void vfs_destroy_inode(struct VFS_INODE* inode);
errorcode_t vfs_get_inode(struct VFS_MOUNTED_FS* fs, void* fsop, struct VFS_INODE** destinode);
errorcode_t vfs_lookup(struct VFS_INODE* cwd, struct VFS_INODE** destinode, const char* dentry);

/* Higher-level interface */
errorcode_t vfs_open(const char* fname, struct VFS_INODE* cwd, struct VFS_FILE* file);
errorcode_t vfs_close(struct VFS_FILE* file);
errorcode_t vfs_read(struct VFS_FILE* file, void* buf, size_t* len);
errorcode_t vfs_write(struct VFS_FILE* file, const void* buf, size_t* len);
errorcode_t vfs_seek(struct VFS_FILE* file, off_t offset);

/* Generic functions */
errorcode_t vfs_generic_lookup(struct VFS_INODE* dirinode, struct VFS_INODE** destinode, const char* dentry);

/* Filesystem specific functions */
size_t vfs_filldirent(void** dirents, size_t* size, const void* fsop, int fsoplen, const char* name, int namelen);

#endif /* __SYS_VFS_H__ */
