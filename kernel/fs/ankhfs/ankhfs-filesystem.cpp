#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/vfs.h>
#include <ananas/vfs/core.h>
#include <ananas/vfs/generic.h>
#include <ananas/vmspace.h>
#include <ananas/lib.h>
#include "filesystem.h"
#include "support.h"

namespace Ananas {
namespace VFS {
size_t GetMaxMountedFilesystems();
extern spinlock_t spl_mountedfs;
extern struct VFS_MOUNTED_FS mountedfs[];

extern spinlock_t spl_fstypes;
extern struct VFS_FILESYSTEMS fstypes;

} // namespace VFS

namespace AnkhFS {
namespace {

constexpr unsigned int subMounts = 1;
constexpr unsigned int subFileSystems = 2;

struct DirectoryEntry fs_entries[] = {
	{ "mounts", make_inum(SS_FileSystem, 0, subMounts) },
	{ "filesystems", make_inum(SS_FileSystem, 0, subFileSystems) },
	{ NULL, 0 }
};

class FileSystemSubSystem : public IAnkhSubSystem
{
public:
	errorcode_t HandleReadDir(struct VFS_FILE* file, void* dirents, size_t* len) override
	{
		return AnkhFS::HandleReadDir(file, dirents, len, fs_entries[0]);
	}

	errorcode_t FillInode(struct VFS_INODE* inode, ino_t inum) override
	{
		if (inum_to_sub(inum) == 0)
			inode->i_sb.st_mode |= S_IFDIR;
		else
			inode->i_sb.st_mode |= S_IFREG;
		return ananas_success();
	}

	errorcode_t HandleRead(struct VFS_FILE* file, void* buf, size_t* len) override
	{
		char result[256]; // XXX
		strcpy(result, "???");

		ino_t inum = file->f_dentry->d_inode->i_inum;
		switch(inum_to_sub(inum)) {
			case subMounts: {
				char* r = result;
				spinlock_lock(&Ananas::VFS::spl_mountedfs);
				for (unsigned int n = 0; n < Ananas::VFS::GetMaxMountedFilesystems(); n++) {
					struct VFS_MOUNTED_FS* fs = &Ananas::VFS::mountedfs[n];
					if ((fs->fs_flags & VFS_FLAG_INUSE) == 0)
						continue;
				 const char* device = "none";
					if (fs->fs_device != nullptr)
						device = fs->fs_device->d_Name;
				  snprintf(r, sizeof(result) - (r - result), "%s %s %x\n", fs->fs_mountpoint, device, fs->fs_flags);
					r += strlen(r);
				}
				spinlock_unlock(&Ananas::VFS::spl_mountedfs);
				break;
			}
			case subFileSystems: {
				char* r = result;
				spinlock_lock(&Ananas::VFS::spl_fstypes);
				LIST_FOREACH(&Ananas::VFS::fstypes, curfs, struct VFS_FILESYSTEM) {
				  snprintf(r, sizeof(result) - (r - result), "%s\n", curfs->fs_name);
					r += strlen(r);
				}
				spinlock_unlock(&Ananas::VFS::spl_fstypes);
				break;
			}
		}

		return AnkhFS::HandleRead(file, buf, len, result);
	}
};

} // unnamed namespace

IAnkhSubSystem& GetFileSystemSubSystem()
{
	static FileSystemSubSystem fileSystemSubSystem;
	return fileSystemSubSystem;
}

} // namespace AnkhFS
} // namespace Ananas

/* vim:set ts=2 sw=2: */
