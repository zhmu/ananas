#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/lib.h"
#include "kernel/handle.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_fstat(Thread* t, handleindex_t hindex, struct stat* buf)
{
	/* Get the handle */
	struct HANDLE* h;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_handle(*t, hindex, &h)
	);

	if (h->h_type != HANDLE_TYPE_FILE)
		return RESULT_MAKE_FAILURE(EBADF);

        struct VFS_FILE* file = &h->h_data.d_vfs_file;
	if (file->f_dentry != NULL) {
		memcpy(buf, &file->f_dentry->d_inode->i_sb, sizeof(struct stat));
	} else {
		return RESULT_MAKE_FAILURE(EINVAL); /* XXX maybe re-think this for devices */
	}

	return Result::Success();
}
