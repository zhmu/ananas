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

static errorcode_t
vfs_exec_obtain(void* priv, void* buf, off_t offset, size_t len)
{
	struct VFS_FILE f;
	memset(&f, 0, sizeof(f));
	f.f_dentry = priv;

	/* kludge: cleanup is handeled by requesting 0 bytes at 0 to buffer NULL */
	if (buf == NULL && offset == 0 && len == 0) {
		dentry_deref(priv);
		return ANANAS_ERROR_OK;
	}

	errorcode_t err = vfs_seek(&f, offset);
	ANANAS_ERROR_RETURN(err);

	size_t amount = len;
	err = vfs_read(&f, buf, &amount);
	ANANAS_ERROR_RETURN(err);

	if (amount != len)
		return ANANAS_ERROR(SHORT_READ);
	return ANANAS_ERROR_OK;
}

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

	/* Grab the path handle for the thread XXX This has to go */
	struct HANDLE* path_handle;
	errorcode_t err = handle_lookup(proc, proc->p_hidx_path, HANDLE_TYPE_FILE, &path_handle);
	KASSERT(err == ANANAS_ERROR_NONE, "sys_execve(): process %p path handle %d invalid: %d", proc, proc->p_hidx_path, err);

	/* First step is to open the file */
	struct VFS_FILE file;
	err = vfs_open(path, &path_handle->h_data.d_vfs_file, &file);
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
	err = exec_launch(t, dentry, &vfs_exec_obtain);
	dentry_deref(dentry);
	return err;
}

/* vim:set ts=2 sw=2: */
