#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

TRACE_SETUP;

namespace
{

Result SetCWD(Process& proc, struct VFS_FILE& file)
{
	DEntry& cwd = *proc.p_cwd;
	DEntry& new_cwd = *file.f_dentry;

	dentry_ref(new_cwd);
	proc.p_cwd = &new_cwd;
	dentry_deref(cwd);
	return Result::Success();
}

} // unnamed namespace

Result
sys_chdir(Thread* t, const char* path)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s'", t, path);
	Process& proc = *t->t_process;
	DEntry* cwd = proc.p_cwd;

	struct VFS_FILE file;
	RESULT_PROPAGATE_FAILURE(
		vfs_open(&proc, path, cwd, &file)
	);

	/* XXX Check if file has a directory */

	// Open succeeded - now update the cwd
	auto result = SetCWD(proc, file);
	vfs_close(&proc, &file);
	return result;
}

Result
sys_fchdir(Thread* t, handleindex_t index)
{
	TRACE(SYSCALL, FUNC, "t=%p, index=%d'", t, index);
	Process& proc = *t->t_process;

	/* Get the handle */
	struct HANDLE* h;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_handle(*t, index, &h)
	);

	if (h->h_type != HANDLE_TYPE_FILE)
		return RESULT_MAKE_FAILURE(EBADF);

	return SetCWD(proc, h->h_data.d_vfs_file);
}

Result
sys_getcwd(Thread* t, char* buf, size_t size)
{
	TRACE(SYSCALL, FUNC, "t=%p, buf=%p, size=%d", t, buf, size);
	Process& proc = *t->t_process;
	DEntry& cwd = *proc.p_cwd;

	if (size == 0)
		return RESULT_MAKE_FAILURE(EINVAL);

	size_t pathLen = dentry_construct_path(NULL, 0, cwd);
	if (size < pathLen + 1)
		return RESULT_MAKE_FAILURE(ERANGE);

	dentry_construct_path(buf, size, cwd);
	return Result::Success();
}
