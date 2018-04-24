#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/types.h"
#include "device.h"
#include "support.h"

TRACE_SETUP;

namespace Ananas {
namespace AnkhFS {
namespace {

struct DirectoryEntry root_entries[] = {
	{ "proc", make_inum(SS_Proc, 0, 0) },
	{ "fs", make_inum(SS_FileSystem, 0, 0) },
	{ "dev", make_inum(SS_Device, 0, Devices::subRoot) },
	{ "devices", make_inum(SS_Device, 0, Devices::subDevices) },
	{ "drivers", make_inum(SS_Device, 0, Devices::subDrivers) },
	{ "kernel", make_inum(SS_Kernel, 0, Devices::subRoot) },
	{ NULL,  0 }
};

class RootProcSubSystem : public IAnkhSubSystem
{
public:
	Result HandleReadDir(struct VFS_FILE* file, void* dirents, size_t* len) override
	{
		return AnkhFS::HandleReadDir(file, dirents, len, root_entries[0]);
	}

	Result FillInode(INode& inode, ino_t inum) override
	{
		inode.i_sb.st_mode |= S_IFDIR;
		return Result::Success();
	}

	Result HandleRead(struct VFS_FILE* file, void* buf, size_t* len) override
	{
		return RESULT_MAKE_FAILURE(EOPNOTSUPP); // XXX would this be expected from a read?
	}

	Result HandleOpen(VFS_FILE& file, Process* p) override
	{
		return Result::Success();
	}

	Result HandleClose(VFS_FILE& file, Process* p) override
	{
		return Result::Success();
	}
};

} // unnamed namespace

IAnkhSubSystem& GetRootSubSystem()
{
	static RootProcSubSystem rootProcSubSystem;
	return rootProcSubSystem;
}

} // namespace AnkhFS
} // namespace Ananas

/* vim:set ts=2 sw=2: */
