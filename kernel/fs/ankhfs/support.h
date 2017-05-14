#ifndef ANANAS_ANKFS_SUPPORT_H
#define ANANAS_ANKFS_SUPPORT_H

#include <ananas/types.h>

namespace Ananas {
namespace AnkhFS {

enum SubSystem {
	SS_Root,
	SS_Proc,
	SS_FileSystem,
	SS_Last // do not use
};

struct DirectoryEntry {
	const char* de_name;
	ino_t de_inum;
};

// Inode numbers are [type] [id] [sub] for us

constexpr ino_t make_inum(SubSystem subsystem, unsigned int id, unsigned int sub) 
{
	return static_cast<ino_t>(subsystem) << 40UL | id << 8UL | sub;
}

constexpr unsigned int inum_to_id(ino_t ino)
{
	return (ino >> 8) & 0xffffffff;
}

constexpr unsigned int inum_to_sub(ino_t ino)
{
	return ino & 0xff;
}

constexpr SubSystem inum_to_subsystem(ino_t ino)
{
	return static_cast<SubSystem>(ino >> 40);
}

class IReadDirCallback
{
public:
	virtual bool FetchNextEntry(char* entry, size_t maxLength, ino_t& inum) = 0;
};

class IAnkhSubSystem
{
public:
	virtual errorcode_t HandleReadDir(struct VFS_FILE* file, void* dirents, size_t* len) = 0;
	virtual errorcode_t HandleRead(struct VFS_FILE* file, void* buf, size_t* len) = 0;
	virtual errorcode_t FillInode(struct VFS_INODE* inode, ino_t inum) = 0;
};

errorcode_t HandleReadDir(struct VFS_FILE* file, void* dirents, size_t* len, IReadDirCallback& callback);
errorcode_t HandleReadDir(struct VFS_FILE* file, void* dirents, size_t* len, const DirectoryEntry& firstEntry, unsigned int id = 0);
errorcode_t HandleRead(struct VFS_FILE* file, void* buf, size_t* len, const char* data);

} // namespace AnkhFS
} // namespace Ananas

#endif // ANANAS_ANKFS_SUPPORT_H
