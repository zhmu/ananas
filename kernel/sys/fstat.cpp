#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/lib.h"
#include "kernel/device.h"
#include "kernel/handle.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

TRACE_SETUP;

Result
sys_fstat(Thread* t, handleindex_t hindex, struct stat* buf)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%d, buf=%p", t, hindex, buf);

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
	} else if (file->f_device != nullptr) {
		memset(buf, 0, sizeof(struct stat));
		buf->st_dev = (dev_t)(uintptr_t)file->f_device;
		buf->st_rdev = buf->st_dev;
		buf->st_ino = 0;
		if (file->f_device->GetCharDeviceOperations() != nullptr)
			buf->st_mode |= S_IFCHR;
		else if (file->f_device->GetBIODeviceOperations() != nullptr)
			buf->st_mode |= S_IFBLK;
		else
			buf->st_mode |= S_IFIFO; // XXX how did you open this?
		buf->st_blksize = PAGE_SIZE; // XXX
	} else {
		// What's this?
		return RESULT_MAKE_FAILURE(EINVAL);
	}

	return Result::Success();
}
