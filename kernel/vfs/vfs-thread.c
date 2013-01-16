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

	/* If there is a parent, try to clone it's parent handle */
	if (parent != NULL) {
		err = handle_clone(thread, parent->t_path_handle, NULL, &thread->t_path_handle);
		if (err != ANANAS_ERROR_NONE) {
			/*
			 * XXX Unable to clone parent's path - what now? Our VFS isn't mature enough
			 * to deal with abandoned handles (or even abandon handles in the first
			 * place), so this should never, ever happen.
			 */
			panic("thread_init(): could not clone root path");
		}
	} else {
		/*
		 * No parent; use / as current path. This will not work in very early
		 * initialiation, but that is fine - our lookup code should know what
		 * to do with the NULL backing inode.
		 */
		err = handle_alloc(HANDLE_TYPE_FILE, thread, &thread->t_path_handle);
		if (err == ANANAS_ERROR_NONE) {
			err = vfs_open("/", NULL, &thread->t_path_handle->h_data.d_vfs_file);
		}
	}

	/* Initialize stdin/out/error - we should actually inherit these XXX */
	err = handle_alloc(HANDLE_TYPE_FILE, thread, (struct HANDLE**)&thread->t_threadinfo->ti_handle_stdin);
	ANANAS_ERROR_RETURN(err);
	err = handle_alloc(HANDLE_TYPE_FILE, thread, (struct HANDLE**)&thread->t_threadinfo->ti_handle_stdout);
	ANANAS_ERROR_RETURN(err);
	err = handle_alloc(HANDLE_TYPE_FILE, thread, (struct HANDLE**)&thread->t_threadinfo->ti_handle_stderr);
	ANANAS_ERROR_RETURN(err);
	/* Hook the new handles to the console */
	((struct HANDLE*)thread->t_threadinfo->ti_handle_stdin)->h_data.d_vfs_file.f_device  = console_tty;
	((struct HANDLE*)thread->t_threadinfo->ti_handle_stdout)->h_data.d_vfs_file.f_device = console_tty;
	((struct HANDLE*)thread->t_threadinfo->ti_handle_stderr)->h_data.d_vfs_file.f_device = console_tty;

	TRACE(THREAD, INFO, "t=%p, stdin=%p, stdout=%p, stderr=%p",
	 thread,
	 thread->t_threadinfo->ti_handle_stdin,
	 thread->t_threadinfo->ti_handle_stdout,
	 thread->t_threadinfo->ti_handle_stderr);

	return ANANAS_ERROR_OK;
}

REGISTER_THREAD_INIT_FUNC(vfs_init_thread);

/* vim:set ts=2 sw=2: */
