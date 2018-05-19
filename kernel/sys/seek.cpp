#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/handle-options.h>
#include "kernel/fd.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_seek(Thread* t, fdindex_t hindex, off_t* offset, int whence)
{
	FD* fd;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_fd(*t, FD_TYPE_FILE, hindex, fd)
	);

        struct VFS_FILE* file = &fd->fd_data.d_vfs_file;
	if (file->f_dentry == NULL)
		return RESULT_MAKE_FAILURE(EINVAL); /* XXX maybe re-think this for devices */

	/* Update the offset */
	off_t new_offset;
	switch(whence) {
		case HCTL_SEEK_WHENCE_SET:
			new_offset = *offset;
			break;
		case HCTL_SEEK_WHENCE_CUR:
			new_offset = file->f_offset + *offset;
			break;
		case HCTL_SEEK_WHENCE_END:
			new_offset = file->f_dentry->d_inode->i_sb.st_size - *offset;
			break;
		default:
			return RESULT_MAKE_FAILURE(EINVAL);
	}
	if (new_offset < 0)
		return RESULT_MAKE_FAILURE(ERANGE);
	if (new_offset > file->f_dentry->d_inode->i_sb.st_size) {
		/* File needs to be grown to accommodate for this offset */
		RESULT_PROPAGATE_FAILURE(
			vfs_grow(file, new_offset)
		);
	}
	file->f_offset = new_offset;
	*offset = new_offset;
	return Result::Success();
}
