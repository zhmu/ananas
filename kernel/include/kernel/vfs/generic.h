#ifndef __ANANAS_VFS_GENERIC_H__
#define __ANANAS_VFS_GENERIC_H__

struct DEntry;
struct INode;
class Result;

Result vfs_generic_lookup(DEntry& dirinode, INode*& destinode, const char* dentry);
Result vfs_generic_read(struct VFS_FILE* file, void* buf, size_t* len);
Result vfs_generic_write(struct VFS_FILE* file, const void* buf, size_t* len);

#endif /* __ANANAS_VFS_GENERIC_H__ */
