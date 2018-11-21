#ifndef __ANANAS_VFS_CORE_H__
#define __ANANAS_VFS_CORE_H__

#include <ananas/types.h>
#include "kernel/vfs/types.h"

#define VFS_MAX_NAME_LEN 255

struct VFS_MOUNTED_FS;
struct VFS_INODE_OPS;
struct VFS_FILESYSTEM_OPS;
struct INode;
struct Procss;
class Result;

/* Low-level interface */
/*
 * Obtains an inode for a given FSOP. The destination inode will have a reference count of
 * at least 2 (one for the caller, and one for the cache).
 */
Result vfs_get_inode(struct VFS_MOUNTED_FS* fs, ino_t inum, INode*& destinode);

/*
 * Retrieves a given block for the given filesystem to a bio.
 */
Result vfs_bget(struct VFS_MOUNTED_FS* fs, blocknr_t block, struct BIO** bio, int flags);

/*
 * Reads a block for the given filesystem to bio.
 */
static inline Result vfs_bread(struct VFS_MOUNTED_FS* fs, blocknr_t block, struct BIO** bio)
{
	return vfs_bget(fs, block, bio, 0);
}

#define VFS_LOOKUP_FLAG_DEFAULT 	0
#define VFS_LOOKUP_FLAG_NO_FOLLOW	1
Result vfs_lookup(DEntry* parent, DEntry*& destentry, const char* dentry, int flags = VFS_LOOKUP_FLAG_DEFAULT);

bool vfs_is_filesystem_sane(struct VFS_MOUNTED_FS* fs);

/* Higher-level interface */
Result vfs_open(Process* p, const char* fname, DEntry* cwd, struct VFS_FILE* file, int lookup_flags = VFS_LOOKUP_FLAG_DEFAULT);
Result vfs_close(Process* p, struct VFS_FILE* file);
Result vfs_read(struct VFS_FILE* file, void* buf, size_t len);
Result vfs_write(struct VFS_FILE* file, const void* buf, size_t len);
Result vfs_seek(struct VFS_FILE* file, off_t offset);
Result vfs_create(DEntry* parent, struct VFS_FILE* destfile, const char* dentry, int mode);
Result vfs_grow(struct VFS_FILE* file, off_t size);
Result vfs_unlink(struct VFS_FILE* file);
Result vfs_rename(struct VFS_FILE* file, DEntry* parent, const char* dest);
Result vfs_ioctl(Process* p, struct VFS_FILE* file, unsigned long request, void* args[]);

/* Filesystem specific functions */
size_t vfs_filldirent(void** dirents, size_t size, ino_t inum, const char* name, int namelen);

Result vfs_init_process(Process& proc);
void vfs_exit_process(Process& proc);

#endif /* __ANANAS_VFS_CORE_H__ */
