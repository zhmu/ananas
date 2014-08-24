#ifndef __ANANAS_VFS_CORE_H__
#define __ANANAS_VFS_CORE_H__

#include <ananas/types.h>
#include <ananas/vfs/types.h>

#define VFS_MAX_NAME_LEN 255

struct VFS_MOUNTED_FS;
struct VFS_INODE_OPS;
struct VFS_FILESYSTEM_OPS;

errorcode_t vfs_mount(const char* from, const char* to, const char* type, void* options);

/* Low-level interface */
/*
 * Obtains an inode for a given FSOP. The destination inode will have a reference count of
 * at least 2 (one for the caller, and one for the cache).
 */
errorcode_t vfs_get_inode(struct VFS_MOUNTED_FS* fs, void* fsop, struct VFS_INODE** destinode);

/*
 * Retrieves a given block for the given filesystem to a bio.
 */
errorcode_t vfs_bget(struct VFS_MOUNTED_FS* fs, blocknr_t block, struct BIO** bio, int flags);

/*
 * Reads a block for the given filesystem to bio.
 */
static inline errorcode_t vfs_bread(struct VFS_MOUNTED_FS* fs, blocknr_t block, struct BIO** bio)
{
	return vfs_bget(fs, block, bio, 0);
}

errorcode_t vfs_lookup(struct DENTRY* parent, struct DENTRY** destentry, const char* dentry);

/* Higher-level interface */
errorcode_t vfs_open(const char* fname, struct VFS_FILE* dir, struct VFS_FILE* file);
errorcode_t vfs_close(struct VFS_FILE* file);
errorcode_t vfs_read(struct VFS_FILE* file, void* buf, size_t* len);
errorcode_t vfs_write(struct VFS_FILE* file, const void* buf, size_t* len);
errorcode_t vfs_seek(struct VFS_FILE* file, off_t offset);
errorcode_t vfs_create(struct DENTRY* parent, struct VFS_FILE* destfile, const char* dentry, int mode);
errorcode_t vfs_grow(struct VFS_FILE* file, off_t size);
errorcode_t vfs_summon(struct VFS_FILE* file, thread_t* t);
errorcode_t vfs_unlink(struct VFS_FILE* file);
errorcode_t vfs_rename(struct VFS_FILE* file, struct DENTRY* parent, const char* dest);

/* Filesystem specific functions */
size_t vfs_filldirent(void** dirents, size_t* size, const void* fsop, int fsoplen, const char* name, int namelen);

#endif /* __ANANAS_VFS_CORE_H__ */
