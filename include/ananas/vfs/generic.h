#ifndef __ANANAS_VFS_GENERIC_H__
#define __ANANAS_VFS_GENERIC_H__

errorcode_t vfs_generic_lookup(struct VFS_INODE* dirinode, struct VFS_INODE** destinode, const char* dentry);
errorcode_t vfs_generic_read(struct VFS_FILE* file, void* buf, size_t* len);
errorcode_t vfs_generic_write(struct VFS_FILE* file, const void* buf, size_t* len);

#endif /* __ANANAS_VFS_GENERIC_H__ */
