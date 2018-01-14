#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"

TRACE_SETUP;

errorcode_t
sys_rename(Thread* t, const char* oldpath, const char* newpath)
{
	TRACE(SYSCALL, FUNC, "t=%p, oldpath='%s' newpath='%s'", t, oldpath, newpath);
	process_t* proc = t->t_process;
	struct DENTRY* cwd = proc->p_cwd;

	struct VFS_FILE file;
	errorcode_t err = vfs_open(oldpath, cwd, &file);
	ANANAS_ERROR_RETURN(err);

	err = vfs_rename(&file, proc->p_cwd, newpath);
	vfs_close(&file);
	return err;
}
