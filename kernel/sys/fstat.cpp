#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/lib.h"
#include "kernel/handle.h"
#include "kernel/trace.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

TRACE_SETUP;

errorcode_t
sys_fstat(Thread* t, handleindex_t hindex, struct stat* buf)
{
	/* Get the handle */
	struct HANDLE* h;
	errorcode_t err = syscall_get_handle(*t, hindex, &h);
	ANANAS_ERROR_RETURN(err);

	if (h->h_type != HANDLE_TYPE_FILE)
		return ANANAS_ERROR(BAD_HANDLE);

        struct VFS_FILE* file = &h->h_data.d_vfs_file;
	if (file->f_dentry != NULL) {
		memcpy(buf, &file->f_dentry->d_inode->i_sb, sizeof(struct stat));
	} else {
		err = ANANAS_ERROR(BAD_OPERATION); /* XXX maybe re-think this for devices */
	}

	return ananas_success();
}
