#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/exec.h>
#include <ananas/lib.h>
#include <ananas/process.h>
#include <ananas/thread.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>
#include <machine/param.h>

TRACE_SETUP;

enum SET_PROC_ATTR {
	A_Args,
	A_Env
};

static void
set_proc_attribute(process_t* process, enum SET_PROC_ATTR attr, const char** list)
{
	char buf[PAGE_SIZE];

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
			process_set_args(process, buf, pos);
			break;
		case A_Env:
			process_set_environment(process, buf, pos);
			break;
		default:
			panic("unexpected attribute %d", attr);
	}
}

errorcode_t
sys_execve(thread_t* t, const char* path, const char** argv, const char** envp)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s', argv='%s', envp='%s'", path, argv, envp);
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
	if (argv != NULL)
		set_proc_attribute(proc, A_Args, argv);
	if (envp != NULL)
		set_proc_attribute(proc, A_Env, envp);

	/* Attempt the launch; if this succeeds, it won't return */
	err = exec_launch(t, dentry);
	dentry_deref(dentry);
	return err;
}

/* vim:set ts=2 sw=2: */
