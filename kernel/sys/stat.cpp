#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"

TRACE_SETUP;

errorcode_t
sys_stat(Thread* t, const char* path, struct stat* buf)
{
	process_t* proc = t->t_process;

	struct VFS_FILE file;
	errorcode_t err = vfs_open(path, proc->p_cwd, &file);
	ANANAS_ERROR_RETURN(err);

	if (file.f_dentry != NULL) {
		memcpy(buf, &file.f_dentry->d_inode->i_sb, sizeof(struct stat));
	} else {
		err = ANANAS_ERROR(BAD_OPERATION); /* XXX maybe re-think this for devices */
	}

	vfs_close(&file);
	return ananas_success();
}
