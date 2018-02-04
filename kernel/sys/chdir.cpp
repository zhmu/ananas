#include <ananas/types.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vm.h"

TRACE_SETUP;

Result
sys_chdir(Thread* t, const char* path)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s'", t, path);
	Process& proc = *t->t_process;
	DEntry* cwd = proc.p_cwd;

	struct VFS_FILE file;
	RESULT_PROPAGATE_FAILURE(
		vfs_open(path, cwd, &file)
	);

	/* XXX Check if file has a directory */

	/* Open succeeded - now update the cwd */
	DEntry& new_cwd = *file.f_dentry;
	dentry_ref(new_cwd);
	proc.p_cwd = &new_cwd;
	dentry_deref(*cwd);

	vfs_close(&file);
	return Result::Success();
}
