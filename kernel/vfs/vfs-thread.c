#include <ananas/types.h>
#include <ananas/console.h>
#include <ananas/error.h>
#include <ananas/handle.h>
#include <ananas/thread.h>
#include <ananas/threadinfo.h>
#include <ananas/trace.h>
#include <ananas/vfs.h>
#include <ananas/lib.h>

TRACE_SETUP;

static errorcode_t
vfs_init_thread(thread_t* thread, thread_t* parent)
{
	errorcode_t err;

	/* Do not bother for kernel threads; these won't use handles anyway */
	if (THREAD_IS_KTHREAD(thread))
		return ANANAS_ERROR_OK;

	/* If there is a parent, try to clone it's parent handles */
	struct HANDLE* path_handle;
	struct HANDLE* stdin_handle;
	struct HANDLE* stdout_handle;
	struct HANDLE* stderr_handle;
	if (parent != NULL) {
		/* Parent should have cloned our handles; we can just copy the indices */
		thread->t_threadinfo->ti_handle_stdin = parent->t_threadinfo->ti_handle_stdin;
		thread->t_threadinfo->ti_handle_stdout = parent->t_threadinfo->ti_handle_stdout;
		thread->t_threadinfo->ti_handle_stderr = parent->t_threadinfo->ti_handle_stderr;
		thread->t_hidx_path = parent->t_hidx_path;
	} else {
		/* Initialize stdin/out/error, so they'll get handle index 0, 1, 2 */
		err = handle_alloc(HANDLE_TYPE_FILE, thread, 0, &stdin_handle, &thread->t_threadinfo->ti_handle_stdin);
		ANANAS_ERROR_RETURN(err);
		err = handle_alloc(HANDLE_TYPE_FILE, thread, 0, &stdout_handle, &thread->t_threadinfo->ti_handle_stdout);
		ANANAS_ERROR_RETURN(err);
		err = handle_alloc(HANDLE_TYPE_FILE, thread, 0, &stderr_handle, &thread->t_threadinfo->ti_handle_stderr);
		ANANAS_ERROR_RETURN(err);

		/* Hook the new handles to the console */
		stdin_handle->h_data.d_vfs_file.f_device  = console_tty;
		stdout_handle->h_data.d_vfs_file.f_device = console_tty;
		stderr_handle->h_data.d_vfs_file.f_device = console_tty;

		/*
		 * No parent; use / as current path. This will not work in very early
		 * initialiation, but that is fine - our lookup code should know what
		 * to do with the NULL backing inode.
		 */
		err = handle_alloc(HANDLE_TYPE_FILE, thread, 0, &path_handle, &thread->t_hidx_path);
		if (err == ANANAS_ERROR_NONE) {
			(void)vfs_open("/", NULL, &path_handle->h_data.d_vfs_file);
		}

	}

	TRACE(THREAD, INFO, "t=%p, stdin=%u, stdout=%u, stderr=%u",
	 thread,
	 thread->t_threadinfo->ti_handle_stdin,
	 thread->t_threadinfo->ti_handle_stdout,
	 thread->t_threadinfo->ti_handle_stderr);

	return ANANAS_ERROR_OK;
}

REGISTER_THREAD_INIT_FUNC(vfs_init_thread);

/* vim:set ts=2 sw=2: */
