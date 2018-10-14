#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/fd.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "kernel-md/md.h"

TRACE_SETUP;

Result
syscall_get_fd(Thread& t, int type, fdindex_t index, FD*& fd)
{
	return fd::Lookup(*t.t_process, index, type, fd);
}

Result
syscall_get_file(Thread& t, fdindex_t index, struct VFS_FILE** out)
{
	FD* fd;
	if (auto result = fd::Lookup(*t.t_process, index, FD_TYPE_FILE, fd); result.IsFailure())
		return result;

	struct VFS_FILE* file = &fd->fd_data.d_vfs_file;
	if (file->f_dentry == NULL && file->f_device == NULL)
		return RESULT_MAKE_FAILURE(EBADF);

	*out = file;
	return Result::Success();
}

Result
syscall_map_string(Thread& t, const void* ptr, const char** out)
{
	auto x = static_cast<const char*>(md::thread::MapThreadMemory(t, (void*)ptr, PAGE_SIZE, VM_FLAG_READ));
	if (x == NULL)
		return RESULT_MAKE_FAILURE(EFAULT);

	/* ensure it's zero terminated */
	for(int i = 0; i < PAGE_SIZE /* XXX */; i++)
		if (x[i] == '\0') {
			*out = x;
			return Result::Success();
		}

	return RESULT_MAKE_FAILURE(EFAULT);
}

Result
syscall_map_buffer(Thread& t, const void* ptr, size_t len, int flags, void** out)
{
	void* x = md::thread::MapThreadMemory(t, (void*)ptr, len, flags);
	if (x == NULL)
	return RESULT_MAKE_FAILURE(EFAULT);

	*out = x;
	return Result::Success();
}

Result
syscall_set_handleindex(Thread& t, fdindex_t* ptr, fdindex_t index)
{
	auto p = static_cast<fdindex_t*>(md::thread::MapThreadMemory(t, (void*)ptr, sizeof(fdindex_t), VM_FLAG_WRITE));
	if (p == NULL)
		return RESULT_MAKE_FAILURE(EFAULT);

	*p = index;
	return Result::Success();
}

Result
syscall_fetch_offset(Thread& t, const void* ptr, off_t* out)
{
	auto o = static_cast<off_t*>(md::thread::MapThreadMemory(t, (void*)ptr, sizeof(off_t), VM_FLAG_READ));
	if (o == NULL)
		return RESULT_MAKE_FAILURE(EFAULT);

	*out = *o;
	return Result::Success();
}

Result
syscall_set_offset(Thread& t, void* ptr, off_t len)
{
	auto o = static_cast<size_t*>(md::thread::MapThreadMemory(t, (void*)ptr, sizeof(size_t), VM_FLAG_WRITE));
	if (o == NULL)
		return RESULT_MAKE_FAILURE(EFAULT);

	*o = len;
	return Result::Success();
}

/* vim:set ts=2 sw=2: */
