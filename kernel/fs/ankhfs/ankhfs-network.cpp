#include <ananas/types.h>
#include <ananas/limits.h>
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/page.h"
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

char hostname[HOST_NAME_MAX] = "unknown"; // XXX This doesn't really belong here

namespace AnkhFS {
namespace {

constexpr unsigned int subHostname = 1;

struct DirectoryEntry fs_entries[] = {
	{ "hostname", make_inum(SS_Network, 0, subHostname)  },
	{ NULL, 0 }
};

class NetworkSubSystem : public IAnkhSubSystem
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
			case subHostname: {
				char* r = result;
				snprintf(r, sizeof(result) - (r - result), "%s\n", hostname);
				break;
			}
		}

		return AnkhFS::HandleRead(file, buf, len, result);
	}

	Result HandleIOControl(struct VFS_FILE* file, unsigned long op, void* args[]) override
	{
		return RESULT_MAKE_FAILURE(EIO);
	}

	Result HandleOpen(VFS_FILE& file, Process* p) override
	{
		return Result::Success();
	}

	Result HandleClose(VFS_FILE& file, Process* p) override
	{
		return Result::Success();
	}

	Result HandleReadLink(INode& inode, void* buf, size_t len) override
	{
		return RESULT_MAKE_FAILURE(EIO);
	}
};

} // unnamed namespace

IAnkhSubSystem& GetNetworkSubSystem()
{
	static NetworkSubSystem networkSubSystem;
	return networkSubSystem;
}

} // namespace AnkhFS
} // namespace Ananas

/* vim:set ts=2 sw=2: */
