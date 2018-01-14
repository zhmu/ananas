#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/handle.h"
#include "kernel/lib.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vm.h"

TRACE_SETUP;

errorcode_t
syscall_get_handle(Thread& t, handleindex_t hindex, struct HANDLE** out)
{
	return handle_lookup(t.t_process, hindex, HANDLE_TYPE_ANY, out);
}

errorcode_t
syscall_get_file(Thread& t, handleindex_t hindex, struct VFS_FILE** out)
{
	struct HANDLE* handle;
	errorcode_t err = handle_lookup(t.t_process, hindex, HANDLE_TYPE_ANY, &handle);
	ANANAS_ERROR_RETURN(err);

	struct VFS_FILE* file = &((struct HANDLE*)handle)->h_data.d_vfs_file;
	if (file->f_dentry == NULL && file->f_device == NULL)
		return ANANAS_ERROR(BAD_HANDLE);

	*out = file;
	return ananas_success();
}

errorcode_t
syscall_map_string(Thread& t, const void* ptr, const char** out)
{
	auto x = static_cast<const char*>(md_map_thread_memory(t, (void*)ptr, PAGE_SIZE, VM_FLAG_READ));
	if (x == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	/* ensure it's zero terminated */
	for(int i = 0; i < PAGE_SIZE /* XXX */; i++)
		if (x[i] == '\0') {
			*out = x;
			return ananas_success();
		}

	return ANANAS_ERROR(BAD_ADDRESS);
}

errorcode_t
syscall_map_buffer(Thread& t, const void* ptr, size_t len, int flags, void** out)
{
	void* x = md_map_thread_memory(t, (void*)ptr, len, flags);
	if (x == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*out = x;
	return ananas_success();
}

errorcode_t
syscall_fetch_size(Thread& t, const void* ptr, size_t* out)
{
	auto s = static_cast<size_t*>(md_map_thread_memory(t, (void*)ptr, sizeof(size_t), VM_FLAG_READ));
	if (s == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*out = *s;
	return ananas_success();
}

errorcode_t
syscall_set_size(Thread& t, void* ptr, size_t len)
{
	auto s = static_cast<size_t*>(md_map_thread_memory(t, (void*)ptr, sizeof(size_t), VM_FLAG_WRITE));
	if (s == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*s = len;
	return ananas_success();
}

errorcode_t
syscall_set_handleindex(Thread& t, handleindex_t* ptr, handleindex_t index)
{
	auto p = static_cast<handleindex_t*>(md_map_thread_memory(t, (void*)ptr, sizeof(handleindex_t), VM_FLAG_WRITE));
	if (p == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*p = index;
	return ananas_success();
}

errorcode_t
syscall_fetch_offset(Thread& t, const void* ptr, off_t* out)
{
	auto o = static_cast<off_t*>(md_map_thread_memory(t, (void*)ptr, sizeof(off_t), VM_FLAG_READ));
	if (o == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*out = *o;
	return ananas_success();
}

errorcode_t
syscall_set_offset(Thread& t, void* ptr, off_t len)
{
	auto o = static_cast<size_t*>(md_map_thread_memory(t, (void*)ptr, sizeof(size_t), VM_FLAG_WRITE));
	if (o == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*o = len;
	return ananas_success();
}

/* vim:set ts=2 sw=2: */
