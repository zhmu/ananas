#include <sys/types.h>
#include <sys/device.h>
#include <sys/stat.h>

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
	 * de_name will be stored directory after the fsop.
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
	int (*mount)(struct VFS_MOUNTED_FS* fs);

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
	int (*read_inode)(struct VFS_INODE* inode, void* fsop);
};

struct VFS_INODE_OPS {
	/*
	 * Reads directory entries, up to numents (dirents must be big enough to hold
	 * them). Returns the number of entries read and updates file's offset.
	 */
	size_t (*readdir)(struct VFS_FILE* file, void* ents, size_t numents);

	/*
	 * Looks up an entry within a directory and returns its inode or NULL
	 * if nothing is found.
	 */
	struct VFS_INODE* (*lookup)(struct VFS_INODE* dirinode, const char* dentry);

	/*
	 * Reads inode data to a buffer, up to len bytes. Returns the number of
	 * bytes read.
	 */
	size_t (*read)(struct VFS_FILE* file, void* buf, size_t len);

	/*
	 * Writes inode data from a buffer, up to len bytes. Returns the number of
	 * bytes written.
	 */
	size_t (*write)(struct VFS_FILE* file, const void* buf, size_t len);
};

void vfs_init();
int vfs_mount(const char* from, const char* to, const char* type, void* options);
struct BIO* vfs_bread(struct VFS_MOUNTED_FS* fs, block_t block, size_t len);


struct VFS_INODE* vfs_alloc_inode(struct VFS_MOUNTED_FS* fs);
void vfs_free_inode(struct VFS_INODE* inode);

struct VFS_INODE* vfs_make_inode(struct VFS_MOUNTED_FS* fs);
void vfs_destroy_inode(struct VFS_INODE* inode);

struct VFS_INODE* vfs_get_inode(struct VFS_MOUNTED_FS* fs, void* fsop);

struct VFS_INODE* vfs_lookup(const char* dentry);

/* Higher-level interface */
int vfs_open(const char* fname, struct VFS_FILE* file);
void vfs_close(struct VFS_FILE* file);
size_t vfs_read(struct VFS_FILE* file, void* buf, size_t len);
size_t vfs_write(struct VFS_FILE* file, const void* buf, size_t len);
int vfs_seek(struct VFS_FILE* file, off_t offset);
size_t vfs_readdir(struct VFS_FILE* file, struct VFS_DIRENT* dirents, size_t numents);
int vfs_filldirent(void** dirents, size_t* size, const void* fsop, int fsoplen, const char* name, int namelen);

#endif /* __SYS_VFS_H__ */
