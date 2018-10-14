#include <ananas/syscalls.h>
#include <ananas/procinfo.h>
#include "kernel/exec.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vmspace.h"
#include "kernel-md/md.h"

TRACE_SETUP;

Result
sys_execve(Thread* t, const char* path, const char** argv, const char** envp)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s'", t, path);
	Process& proc = *t->t_process;

	/* First step is to open the file */
	struct VFS_FILE file;
	RESULT_PROPAGATE_FAILURE(
		vfs_open(&proc, path, proc.p_cwd, &file)
	);

	/*
	 * Add a ref to the dentry; we'll be throwing away 'file' soon but we need to
	 * keep the backing item intact.
	 */
	DEntry& dentry = *file.f_dentry;
	dentry_ref(dentry);
	vfs_close(&proc, &file);

	/*
	 * Create a new vmspace to execute in; if the exec() works, we'll use it to
	 * override our current vmspace.
	 */
	VMSpace* vmspace = NULL;
	Result result = vmspace_create(vmspace);
	if (result.IsFailure()) {
		dentry_deref(dentry);
		goto fail;
	}

	/*
	 * Attempt to load the executable; if this fails, we won't have destroyed
	 * anything we cannot free. Note that we can free our dentry ref because
	 * exec_load() manages that all by itself.
	 */
	IExecutor* exec;
	void* auxargs;
	result = exec_load(*vmspace, dentry, exec, auxargs);
	dentry_deref(dentry);
	if (result.IsFailure())
		goto fail;

	/*
	 * Loading went okay; we can now set the new thread name (we won't be able to
	 * access argv after the vmspace_clone() so best do it here)
	 */
	exec->PrepareForExecute(*vmspace, *t, auxargs, argv, envp);

	/* Copy the new vmspace to the destination */
	result = vmspace->Clone(*proc.p_vmspace, VMSPACE_CLONE_EXEC);
	KASSERT(result.IsSuccess(), "unable to clone exec vmspace: %d", result.AsStatusCode());
	vmspace_destroy(*vmspace);
	return Result::Success();

fail:
	if (vmspace != nullptr)
		vmspace_destroy(*vmspace);
	return result;
}

/* vim:set ts=2 sw=2: */
