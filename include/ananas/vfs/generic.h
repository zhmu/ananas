#ifndef __ANANAS_VFS_GENERIC_H__
#define __ANANAS_VFS_GENERIC_H__

errorcode_t vfs_generic_lookup(struct VFS_INODE* dirinode, struct VFS_INODE** destinode, const char* dentry);

#endif /* __ANANAS_VFS_GENERIC_H__ */
