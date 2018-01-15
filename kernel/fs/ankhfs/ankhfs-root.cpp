#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/lib.h"
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
	{ NULL,  0 }
};

class RootProcSubSystem : public IAnkhSubSystem
{
public:
	errorcode_t HandleReadDir(struct VFS_FILE* file, void* dirents, size_t* len) override
	{
		return AnkhFS::HandleReadDir(file, dirents, len, root_entries[0]);
	}

	errorcode_t FillInode(INode& inode, ino_t inum) override
	{
		inode.i_sb.st_mode |= S_IFDIR;
		return ananas_success();
	}

	errorcode_t HandleRead(struct VFS_FILE* file, void* buf, size_t* len) override
	{
		return ANANAS_ERROR(UNSUPPORTED);
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
