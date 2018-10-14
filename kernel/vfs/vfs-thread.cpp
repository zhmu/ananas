#include <ananas/types.h>
#include "kernel/console.h"
#include "kernel/fd.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"

TRACE_SETUP;

static Result
vfs_init_process(Process& proc)
{
	TRACE(THREAD, INFO, "proc=%p", &proc);

	/* If there is a parent, try to clone it's parent handles */
	Process* parent = proc.p_parent;
	if (parent != nullptr) {
		/* Parent should have cloned our fd's - only need to grab the work directory here */
		proc.p_cwd = parent->p_cwd;
		if (proc.p_cwd != nullptr)
			dentry_ref(*proc.p_cwd);
	} else {
		/* Initialize stdin/out/error, so they'll get handle index 0, 1, 2 */
		FD* stdin_fd;
		fdindex_t idx;
		RESULT_PROPAGATE_FAILURE(
			fd::Allocate(FD_TYPE_FILE, proc, 0, stdin_fd, idx)
		);
		KASSERT(idx == 0, "stdin index mismatch (%d)", idx);

		FD* stdout_fd;
		RESULT_PROPAGATE_FAILURE(
			fd::Allocate(FD_TYPE_FILE, proc, 0, stdout_fd, idx)
		);
		KASSERT(idx == 1, "stdout index mismatch (%d)", idx);

		FD* stderr_fd;
		RESULT_PROPAGATE_FAILURE(
			fd::Allocate(FD_TYPE_FILE, proc, 0, stderr_fd, idx)
		);
		KASSERT(idx == 2, "stderr index mismatch (%d)", idx);

		// Hook the new handles to the console
		stdin_fd->fd_data.d_vfs_file.f_device  = console_tty;
		stdout_fd->fd_data.d_vfs_file.f_device = console_tty;
		stderr_fd->fd_data.d_vfs_file.f_device = console_tty;

		/* Use / as current path - by the time we create processes, we should have a workable VFS */
		RESULT_PROPAGATE_FAILURE(
			vfs_lookup(NULL, proc.p_cwd, "/")
		);
	}

	return Result::Success();
}

static Result
vfs_exit_process(Process& proc)
{
	TRACE(THREAD, INFO, "proc=%p", &proc);

	if (proc.p_cwd != nullptr) {
		dentry_deref(*proc.p_cwd);
		proc.p_cwd = nullptr;
	}

	return Result::Success();
}

REGISTER_PROCESS_INIT_FUNC(vfs_init_process);
REGISTER_PROCESS_EXIT_FUNC(vfs_exit_process);

/* vim:set ts=2 sw=2: */
