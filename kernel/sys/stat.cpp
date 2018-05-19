#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/device.h"
#include "kernel/fd.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "syscall.h"

TRACE_SETUP;

namespace
{

Result
fill_stat_buf(struct VFS_FILE* file, struct stat* buf)
{
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

Result
do_stat(Thread* t, const char* path, struct stat* buf, int lookup_flags)
{
	Process& proc = *t->t_process;

	struct VFS_FILE file;
	RESULT_PROPAGATE_FAILURE(
		vfs_open(&proc, path, proc.p_cwd, &file, lookup_flags)
	);

	auto result = fill_stat_buf(&file, buf);
	vfs_close(&proc, &file);
	return result;
}

} // unnamed namespace

Result
sys_stat(Thread* t, const char* path, struct stat* buf)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s' buf=%p", t, path, buf);
	return do_stat(t, path, buf, VFS_LOOKUP_FLAG_DEFAULT);
}

Result
sys_lstat(Thread* t, const char* path, struct stat* buf)
{
	TRACE(SYSCALL, FUNC, "t=%p, path='%s' buf=%p", t, path, buf);
	return do_stat(t, path, buf, VFS_LOOKUP_FLAG_NO_FOLLOW);
}

Result
sys_fstat(Thread* t, fdindex_t hindex, struct stat* buf)
{
	TRACE(SYSCALL, FUNC, "t=%p, hindex=%d, buf=%p", t, hindex, buf);

	/* Get the handle */
	FD* fd;
	RESULT_PROPAGATE_FAILURE(
		syscall_get_fd(*t, FD_TYPE_FILE, hindex, fd)
	);

        VFS_FILE* file = &fd->fd_data.d_vfs_file;
	return fill_stat_buf(file, buf);
}
