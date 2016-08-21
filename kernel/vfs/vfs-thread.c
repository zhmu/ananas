#include <ananas/types.h>
#include <ananas/console.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/process.h>
#include <ananas/procinfo.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>
#include <ananas/lib.h>

TRACE_SETUP;

static errorcode_t
vfs_init_process(process_t* proc)
{
	errorcode_t err;

	/* If there is a parent, try to clone it's parent handles */
	struct HANDLE* path_handle;
	struct HANDLE* stdin_handle;
	struct HANDLE* stdout_handle;
	struct HANDLE* stderr_handle;
	process_t* parent = proc->p_parent;
	if (parent != NULL) {
		/* Parent should have cloned our handles; we can just copy the indices */
		proc->p_info->pi_handle_stdin = parent->p_info->pi_handle_stdin;
		proc->p_info->pi_handle_stdout = parent->p_info->pi_handle_stdout;
		proc->p_info->pi_handle_stderr = parent->p_info->pi_handle_stderr;
		proc->p_hidx_path = parent->p_hidx_path;
	} else {
		/* Initialize stdin/out/error, so they'll get handle index 0, 1, 2 */
		err = handle_alloc(HANDLE_TYPE_FILE, proc, 0, &stdin_handle, &proc->p_info->pi_handle_stdin);
		ANANAS_ERROR_RETURN(err);

		err = handle_alloc(HANDLE_TYPE_FILE, proc, 0, &stdout_handle, &proc->p_info->pi_handle_stdout);
		ANANAS_ERROR_RETURN(err);
		err = handle_alloc(HANDLE_TYPE_FILE, proc, 0, &stderr_handle, &proc->p_info->pi_handle_stderr);
		ANANAS_ERROR_RETURN(err);

		/* Hook the new handles to the console */
		stdin_handle->h_data.d_vfs_file.f_device  = console_tty;
		stdout_handle->h_data.d_vfs_file.f_device = console_tty;
		stderr_handle->h_data.d_vfs_file.f_device = console_tty;

		/* Use / as current path - by the time we create processes, we should have a workable VFS */
		err = handle_alloc(HANDLE_TYPE_FILE, proc, 0, &path_handle, &proc->p_hidx_path);
		ANANAS_ERROR_RETURN(err);
		err = vfs_open("/", NULL, &path_handle->h_data.d_vfs_file);
		ANANAS_ERROR_RETURN(err);
	}

	TRACE(THREAD, INFO, "t=%p, stdin=%u, stdout=%u, stderr=%u",
	 proc,
	 proc->p_info->pi_handle_stdin,
	 proc->p_info->pi_handle_stdout,
	 proc->p_info->pi_handle_stderr);

	return ANANAS_ERROR_OK;
}

REGISTER_PROCESS_INIT_FUNC(vfs_init_process);

/* vim:set ts=2 sw=2: */
