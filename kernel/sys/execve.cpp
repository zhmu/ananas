#include <ananas/errno.h>
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

	// Prepare the executable; this verifies whether we have a shot at executing
	// this file.
	auto exec = exec_prepare(dentry);
	if (exec == nullptr) {
		dentry_deref(dentry);
		return RESULT_MAKE_FAILURE(ENOEXEC);
	}

	// Grab the vmspace of the process and clean it; this should only leave the
	// kernel stack, which we're currently using - any other mappings are gone
	auto& vmspace = *proc.p_vmspace;
	vmspace.PrepareForExecute();

	/*
	 * Attempt to load the executable; if this fails, our vmspace will be in some
	 * inconsistent state and we have no choice but to destroy the process.
	 */
	void* auxargs;
	if (auto result = exec->Load(vmspace, dentry, auxargs); result.IsFailure()) {
		panic("TODO deal with exec load failures");
		dentry_deref(dentry);
		return result;
	}

	/*
	 * Loading went okay; we can now set the new thread name (we won't be able to
	 * access argv after the vmspace_clone() so best do it here)
	 */
	exec->PrepareForExecute(vmspace, *t, auxargs, argv, envp);
	dentry_deref(dentry);
	return Result::Success();
}

/* vim:set ts=2 sw=2: */
