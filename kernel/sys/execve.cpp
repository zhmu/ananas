#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/procinfo.h>
#include "kernel/exec.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vmspace.h"

TRACE_SETUP;

enum SET_PROC_ATTR {
	A_Args,
	A_Env
};

static errorcode_t
set_proc_attribute(process_t* process, enum SET_PROC_ATTR attr, const char** list)
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
			return process_set_args(process, buf, pos);
		case A_Env:
			return process_set_environment(process, buf, pos);
		default:
			panic("unexpected attribute %d", attr);
	}
}

errorcode_t
sys_execve(thread_t* t, const char* path, const char** argv, const char** envp)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s'", t, path);
	process_t* proc = t->t_process;

	/* First step is to open the file */
	struct VFS_FILE file;
	errorcode_t err = vfs_open(path, proc->p_cwd, &file);
	ANANAS_ERROR_RETURN(err);

	/*
	 * Add a ref to the dentry; we'll be throwing away 'f' soon but we need to
	 * keep the backing item intact.
	 */
	struct DENTRY* dentry = file.f_dentry;
	dentry_ref(dentry);
	vfs_close(&file);

	/* XXX Do we inherit correctly here? */
	vmspace_t* vmspace = NULL;
	if (argv != NULL) {
		err = set_proc_attribute(proc, A_Args, argv);
		if (ananas_is_failure(err)) {
			dentry_deref(dentry);
			goto fail;
		}
	}
	if (envp != NULL) {
		err = set_proc_attribute(proc, A_Env, envp);
		if (ananas_is_failure(err)) {
			dentry_deref(dentry);
			goto fail;
		}
	}

	/*
	 * Create a new vmspace to execute in; if the exec() works, we'll use it to
	 * override our current vmspace.
	 */
	err = vmspace_create(&vmspace);
	if (ananas_is_failure(err)) {
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
	err = exec_load(vmspace, dentry, &exec_addr, &exec_arg);
	dentry_deref(dentry);
	if (ananas_is_failure(err))
		goto fail;

	/*
	 * Loading went okay; we can now set the new thread name (we won't be able to
	 * access argv after the vmspace_clone() so best do it here)
	 */
	if (argv != NULL && argv[0] != NULL)
		thread_set_name(t, argv[0]);

	/* Copy the new vmspace to the destination */
	err = vmspace_clone(vmspace, proc->p_vmspace, VMSPACE_CLONE_EXEC);
	KASSERT(ananas_is_success(err), "unable to clone exec vmspace: %d", err);
	vmspace_destroy(vmspace);

	/* Now force a full return into the new thread state */
	md_setup_post_exec(t, exec_addr, exec_arg);
	return ananas_success();

fail:
	if (vmspace != NULL)
		vmspace_destroy(vmspace);
	return err;
}

/* vim:set ts=2 sw=2: */
