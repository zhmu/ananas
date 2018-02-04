#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_fchdir(Thread* t, handleindex_t index)
{
	TRACE(SYSCALL, FUNC, "t=%p, index=%d'", t, index);
	Process& proc = *t->t_process;
	DEntry& cwd = *proc.p_cwd;

	/* Get the handle */
	struct HANDLE* h;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_handle(*t, index, &h)
	);

	if (h->h_type != HANDLE_TYPE_FILE)
		return RESULT_MAKE_FAILURE(EBADF);

        struct VFS_FILE* file = &h->h_data.d_vfs_file;
	DEntry& new_cwd = *file->f_dentry;
	dentry_ref(new_cwd);
	proc.p_cwd = &new_cwd;
	dentry_deref(cwd);

	return Result::Success();
}
