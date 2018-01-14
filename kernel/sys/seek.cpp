#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/handle-options.h>
#include "kernel/handle.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "syscall.h"

TRACE_SETUP;

errorcode_t
sys_seek(Thread* t, handleindex_t hindex, off_t* offset, int whence)
{
	/* Get the handle */
	struct HANDLE* h;
	errorcode_t err = syscall_get_handle(*t, hindex, &h);
	ANANAS_ERROR_RETURN(err);

	if (h->h_type != HANDLE_TYPE_FILE)
		return ANANAS_ERROR(BAD_HANDLE);
        struct VFS_FILE* file = &h->h_data.d_vfs_file;
	if (file->f_dentry == NULL)
		return ANANAS_ERROR(BAD_OPERATION); /* XXX maybe re-think this for devices */

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
			return ANANAS_ERROR(BAD_TYPE);
	}
	if (new_offset < 0)
		return ANANAS_ERROR(BAD_RANGE);
	if (new_offset > file->f_dentry->d_inode->i_sb.st_size) {
		/* File needs to be grown to accommodate for this offset */
		err = vfs_grow(file, new_offset);
		ANANAS_ERROR_RETURN(err);
	}
	file->f_offset = new_offset;
	*offset = new_offset;
	return ananas_success();
}
