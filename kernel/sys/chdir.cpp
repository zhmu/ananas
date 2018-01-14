#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vm.h"

TRACE_SETUP;

errorcode_t
sys_chdir(Thread* t, const char* path)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s'", t, path);
	process_t* proc = t->t_process;
	DEntry* cwd = proc->p_cwd;

	struct VFS_FILE file;
	errorcode_t err = vfs_open(path, cwd, &file);
	ANANAS_ERROR_RETURN(err);

	/* XXX Check if file has a directory */

	/* Open succeeded - now update the cwd */
	DEntry& new_cwd = *file.f_dentry;
	dentry_ref(new_cwd);
	proc->p_cwd = &new_cwd;
	dentry_deref(*cwd);

	vfs_close(&file);
	return err;
}
