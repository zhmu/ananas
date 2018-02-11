#ifndef __ANANAS_VFS_GENERIC_H__
#define __ANANAS_VFS_GENERIC_H__

struct DEntry;
struct INode;
class Result;

Result vfs_generic_lookup(DEntry& dirinode, INode*& destinode, const char* dentry);
Result vfs_generic_read(struct VFS_FILE* file, void* buf, size_t* len);
Result vfs_generic_write(struct VFS_FILE* file, const void* buf, size_t* len);
Result vfs_generic_follow_link(struct VFS_FILE* file, DEntry& base, DEntry*& result);

#endif /* __ANANAS_VFS_GENERIC_H__ */
