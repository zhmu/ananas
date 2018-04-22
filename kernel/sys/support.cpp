#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/handle.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "kernel-md/md.h"

TRACE_SETUP;

Result
syscall_get_handle(Thread& t, handleindex_t hindex, struct HANDLE** out)
{
	return handle_lookup(*t.t_process, hindex, HANDLE_TYPE_ANY, out);
}

Result
syscall_get_file(Thread& t, handleindex_t hindex, struct VFS_FILE** out)
{
	struct HANDLE* handle;
	RESULT_PROPAGATE_FAILURE(
		handle_lookup(*t.t_process, hindex, HANDLE_TYPE_ANY, &handle)
	);

	struct VFS_FILE* file = &((struct HANDLE*)handle)->h_data.d_vfs_file;
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
syscall_fetch_size(Thread& t, const void* ptr, size_t* out)
{
	auto s = static_cast<size_t*>(md::thread::MapThreadMemory(t, (void*)ptr, sizeof(size_t), VM_FLAG_READ));
	if (s == NULL)
		return RESULT_MAKE_FAILURE(EFAULT);

	*out = *s;
	return Result::Success();
}

Result
syscall_set_size(Thread& t, void* ptr, size_t len)
{
	auto s = static_cast<size_t*>(md::thread::MapThreadMemory(t, (void*)ptr, sizeof(size_t), VM_FLAG_WRITE));
	if (s == NULL)
		return RESULT_MAKE_FAILURE(EFAULT);

	*s = len;
	return Result::Success();
}

Result
syscall_set_handleindex(Thread& t, handleindex_t* ptr, handleindex_t index)
{
	auto p = static_cast<handleindex_t*>(md::thread::MapThreadMemory(t, (void*)ptr, sizeof(handleindex_t), VM_FLAG_WRITE));
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
