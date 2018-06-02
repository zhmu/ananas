#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/generic.h"
#include "device.h"
#include "filesystem.h"
#include "support.h"

TRACE_SETUP;

namespace Ananas {
namespace VFS {
size_t GetMaxMountedFilesystems();
extern Spinlock spl_mountedfs;
extern struct VFS_MOUNTED_FS mountedfs[];

extern Spinlock spl_fstypes;
extern VFSFileSystemList fstypes;

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
	Result HandleReadDir(struct VFS_FILE* file, void* dirents, size_t len) override
	{
		return AnkhFS::HandleReadDir(file, dirents, len, fs_entries[0]);
	}

	Result FillInode(INode& inode, ino_t inum) override
	{
		if (inum_to_sub(inum) == 0)
			inode.i_sb.st_mode |= S_IFDIR;
		else
			inode.i_sb.st_mode |= S_IFREG;
		return Result::Success();
	}

	Result HandleRead(struct VFS_FILE* file, void* buf, size_t len) override
	{
		char result[256]; // XXX
		strcpy(result, "???");

		ino_t inum = file->f_dentry->d_inode->i_inum;
		switch(inum_to_sub(inum)) {
			case subMounts: {
				SpinlockGuard g(Ananas::VFS::spl_mountedfs);

				char* r = result;
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
				break;
			}
			case subFileSystems: {
				SpinlockGuard g(Ananas::VFS::spl_fstypes);

				char* r = result;
				for(auto& curfs: Ananas::VFS::fstypes) {
				  snprintf(r, sizeof(result) - (r - result), "%s\n", curfs.fs_name);
					r += strlen(r);
				}
				break;
			}
		}

		return AnkhFS::HandleRead(file, buf, len, result);
	}

	Result HandleOpen(VFS_FILE& file, Process* p) override
	{
		return Result::Success();
	}

	Result HandleClose(VFS_FILE& file, Process* p) override
	{
		return Result::Success();
	}

	Result HandleIOControl(struct VFS_FILE* file, unsigned long op, void* args[]) override
	{
		return RESULT_MAKE_FAILURE(EIO);
	}

	Result HandleReadLink(INode& inode, void* buf, size_t len) override
	{
		return RESULT_MAKE_FAILURE(EIO);
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
