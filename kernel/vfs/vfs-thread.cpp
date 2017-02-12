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
	TRACE(THREAD, INFO, "proc=%p", proc);

	errorcode_t err;

	/* If there is a parent, try to clone it's parent handles */
	struct HANDLE* stdin_handle;
	struct HANDLE* stdout_handle;
	struct HANDLE* stderr_handle;
	process_t* parent = proc->p_parent;
	if (parent != NULL) {
		/* Parent should have cloned our handles - only need to grab the work directory here */
		proc->p_cwd = parent->p_cwd;
		if (proc->p_cwd != NULL)
			dentry_ref(proc->p_cwd);
	} else {
		/* Initialize stdin/out/error, so they'll get handle index 0, 1, 2 */
		handleindex_t hidx;
		err = handle_alloc(HANDLE_TYPE_FILE, proc, 0, &stdin_handle, &hidx);
		ANANAS_ERROR_RETURN(err);
		KASSERT(hidx == 0, "stdin index mismatch (%d)", hidx);

		err = handle_alloc(HANDLE_TYPE_FILE, proc, 0, &stdout_handle, &hidx);
		ANANAS_ERROR_RETURN(err);
		KASSERT(hidx == 1, "stdout index mismatch (%d)", hidx);

		err = handle_alloc(HANDLE_TYPE_FILE, proc, 0, &stderr_handle, &hidx);
		ANANAS_ERROR_RETURN(err);
		KASSERT(hidx == 2, "stderr index mismatch (%d)", hidx);

		/* Hook the new handles to the console */
		stdin_handle->h_data.d_vfs_file.f_device  = console_tty;
		stdout_handle->h_data.d_vfs_file.f_device = console_tty;
		stderr_handle->h_data.d_vfs_file.f_device = console_tty;

		/* Use / as current path - by the time we create processes, we should have a workable VFS */
		err = vfs_lookup(NULL, &proc->p_cwd, "/");
		ANANAS_ERROR_RETURN(err);
	}

	return ananas_success();
}

REGISTER_PROCESS_INIT_FUNC(vfs_init_process);

/* vim:set ts=2 sw=2: */
