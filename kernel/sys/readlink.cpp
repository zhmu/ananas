#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vm.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_readlink(Thread* t, const char* path, char* buf, size_t* buflen)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%p'", t, path);
	Process& proc = *t->t_process;
	DEntry* cwd = proc.p_cwd;

	// Fetch the size operand
	size_t size;
	RESULT_PROPAGATE_FAILURE(
		syscall_fetch_size(*t, buflen, &size)
	);

	// Attempt to map the buffer write-only
	void* buffer;
	RESULT_PROPAGATE_FAILURE(
		syscall_map_buffer(*t, buf, size, VM_FLAG_WRITE, &buffer)
	);

	// Open the link
	struct VFS_FILE file;
	RESULT_PROPAGATE_FAILURE(
		vfs_open(&proc, path, cwd, &file, VFS_LOOKUP_FLAG_NO_FOLLOW)
	);

	// And copy the contents
	auto result = vfs_read(&file, buf, &size);
	vfs_close(&proc, &file);
	if (result.IsFailure())
		return result;

	/* Finally, inform the user of the length read - the read went OK */
	RESULT_PROPAGATE_FAILURE(
		syscall_set_size(*t, buflen, size)
	);

	TRACE(SYSCALL, FUNC, "t=%p, success: size=%u", t, size);
	return Result::Success();
}

