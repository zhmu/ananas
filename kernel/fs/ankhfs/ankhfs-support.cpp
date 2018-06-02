#include <ananas/types.h>
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/generic.h"
#include "support.h"

namespace Ananas {
namespace AnkhFS {

Result
HandleReadDir(struct VFS_FILE* file, void* dirents, size_t len, IReadDirCallback& callback)
{
	size_t written = 0, left = len;

	int cur_offset = 0;
	char entry[64]; // XXX
	ino_t inum;
	while(callback.FetchNextEntry(entry, sizeof(entry), inum)) {
		size_t entry_length = strlen(entry);
		bool skip = cur_offset < file->f_offset;
		cur_offset += sizeof(struct VFS_DIRENT) + entry_length;
		if(skip)
			continue;

		auto filled = vfs_filldirent(&dirents, left, inum, entry, entry_length);
		if(!filled)
			break; // out of space
		file->f_offset += filled;
		written += filled;
		left -= filled;
	}

	return Result::Success(written);
}

Result
HandleReadDir(struct VFS_FILE* file, void* dirents, size_t len, const DirectoryEntry& firstEntry, unsigned int id)
{
	struct FetchDirectoryEntry : IReadDirCallback {
		FetchDirectoryEntry(const DirectoryEntry& de, int id)
			: currentEntry(&de), newId(id)
		{
		}

		bool FetchNextEntry(char* entry, size_t maxLength, ino_t& inum) override {
			if (currentEntry->de_name == nullptr)
				return false;
			strncpy(entry, currentEntry->de_name, maxLength);
			inum = currentEntry->de_inum;
			inum |= make_inum(static_cast<SubSystem>(0), newId, 0);
			currentEntry++;
			return true;
		}

		const DirectoryEntry* currentEntry;
		unsigned int newId;
	};

	FetchDirectoryEntry fetcher(firstEntry, id);
	return HandleReadDir(file, dirents, len, fetcher);
}

Result
HandleRead(struct VFS_FILE* file, void* buf, size_t len, const char* data)
{
	size_t dataLength = strlen(data);
	if (file->f_offset >= dataLength)
		return Result::Success(0);

	size_t left = dataLength - file->f_offset;
	if (left > len)
		left = len;
	memcpy(buf, &data[file->f_offset], left);

	file->f_offset += left;
	return Result::Success(left);
}

} // namespace AnkhFS
} // namespace Ananas

/* vim:set ts=2 sw=2: */
