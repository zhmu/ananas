#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"

TRACE_SETUP;

Result
sys_stat(Thread* t, const char* path, struct stat* buf)
{
	Process& proc = *t->t_process;

	struct VFS_FILE file;
	RESULT_PROPAGATE_FAILURE(
		vfs_open(path, proc.p_cwd, &file)
	);

	auto result = Result::Success();
	if (file.f_dentry != NULL) {
		memcpy(buf, &file.f_dentry->d_inode->i_sb, sizeof(struct stat));
	} else {
		result = RESULT_MAKE_FAILURE(EINVAL); // XXX maybe re-think this for devices
	}

	vfs_close(&file);
	return result;
}
