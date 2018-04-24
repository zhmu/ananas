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

enum SET_PROC_ATTR {
	A_Args,
	A_Env
};

static Result
set_proc_attribute(Process& process, enum SET_PROC_ATTR attr, const char** list)
{
	char buf[PROCINFO_ENV_LENGTH];

	/* Convert list to a \0-separated string, \0\0-terminated */
	int arg_len = 2; /* \0\0-terminator */
	for (const char** p = list; *p != NULL; p++) {
		arg_len += strlen(*p) + 1; /* \0 */
	}
	KASSERT(arg_len < sizeof(buf), "buffer too large (calculated %d, have %d)", arg_len, sizeof(buf));

	/* Copy the items over */
	int pos = 0;
	memset(buf, 0, sizeof(buf));
	for (const char** p = list; *p != NULL; p++) {
		strcpy(&buf[pos], *p);
		pos += strlen(*p) + 1;
	}

	switch(attr) {
		case A_Args:
			return process.SetArguments(buf, pos);
		case A_Env:
			return process.SetEnvironment(buf, pos);
		default:
			panic("unexpected attribute %d", attr);
	}
}

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

	/* XXX Do we inherit correctly here? */
	if (argv != NULL) {
		if (auto result = set_proc_attribute(proc, A_Args, argv); result.IsFailure()) {
			dentry_deref(dentry);
			return result;
		}
	}
	if (envp != NULL) {
		if (auto result = set_proc_attribute(proc, A_Env, envp); result.IsFailure()) {
			dentry_deref(dentry);
			return result;
		}
	}

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
	addr_t exec_addr;
	register_t exec_arg;
	result = exec_load(*vmspace, dentry, &exec_addr, &exec_arg);
	dentry_deref(dentry);
	if (result.IsFailure())
		goto fail;

	/*
	 * Loading went okay; we can now set the new thread name (we won't be able to
	 * access argv after the vmspace_clone() so best do it here)
	 */
	if (argv != nullptr && argv[0] != nullptr)
		t->SetName(argv[0]);

	/* Copy the new vmspace to the destination */
	result = vmspace->Clone(*proc.p_vmspace, VMSPACE_CLONE_EXEC);
	KASSERT(result.IsSuccess(), "unable to clone exec vmspace: %d", result.AsStatusCode());
	vmspace_destroy(*vmspace);

	/* Now force a full return into the new thread state */
	md::thread::SetupPostExec(*t, exec_addr, exec_arg);
	return Result::Success();

fail:
	if (vmspace != nullptr)
		vmspace_destroy(*vmspace);
	return result;
}

/* vim:set ts=2 sw=2: */
