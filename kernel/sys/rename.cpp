#include <ananas/types.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"

TRACE_SETUP;

Result
sys_rename(Thread* t, const char* oldpath, const char* newpath)
{
	TRACE(SYSCALL, FUNC, "t=%p, oldpath='%s' newpath='%s'", t, oldpath, newpath);
	Process& proc = *t->t_process;
	DEntry* cwd = proc.p_cwd;

	struct VFS_FILE file;
	RESULT_PROPAGATE_FAILURE(
		vfs_open(oldpath, cwd, &file)
	);

	auto result = vfs_rename(&file, cwd, newpath);
	vfs_close(&file);
	return result;
}
