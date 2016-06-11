#include <machine/param.h>
#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/handle.h>
#include <ananas/thread.h>
#include <ananas/trace.h>

TRACE_SETUP;

errorcode_t
syscall_get_handle(thread_t* t, handleindex_t hindex, struct HANDLE** out)
{
	return handle_lookup(t, hindex, HANDLE_TYPE_ANY, out);
}

errorcode_t
syscall_get_file(thread_t* t, handleindex_t hindex, struct VFS_FILE** out)
{
	struct HANDLE* handle;
	errorcode_t err = handle_lookup(t, hindex, HANDLE_TYPE_ANY, &handle);
	ANANAS_ERROR_RETURN(err);

	struct VFS_FILE* file = &((struct HANDLE*)handle)->h_data.d_vfs_file;
	if (file->f_dentry == NULL && file->f_device == NULL)
		return ANANAS_ERROR(BAD_HANDLE);

	*out = file;
	return ANANAS_ERROR_OK;
}

errorcode_t
syscall_map_string(thread_t* t, const void* ptr, const char** out)
{
	const char* x = md_map_thread_memory(t, (void*)ptr, PAGE_SIZE, THREAD_MAP_READ);
	if (x == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	/* ensure it's zero terminated */
	for(int i = 0; i < PAGE_SIZE /* XXX */; i++)
		if (x[i] == '\0') {
			*out = x;
			return ANANAS_ERROR_OK;
		}

	return ANANAS_ERROR(BAD_ADDRESS);
}

errorcode_t
syscall_map_buffer(thread_t* t, const void* ptr, size_t len, int flags, void** out)
{
	void* x = md_map_thread_memory(t, (void*)ptr, len, flags);
	if (x == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*out = x;
	return ANANAS_ERROR_OK;
}

errorcode_t
syscall_fetch_size(thread_t* t, const void* ptr, size_t* out)
{
	size_t* s = md_map_thread_memory(t, (void*)ptr, sizeof(size_t), THREAD_MAP_READ);
	if (s == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*out = *s;
	return ANANAS_ERROR_OK;
}

errorcode_t
syscall_set_size(thread_t* t, void* ptr, size_t len)
{
	size_t* s = md_map_thread_memory(t, (void*)ptr, sizeof(size_t), THREAD_MAP_WRITE);
	if (s == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*s = len;
	return ANANAS_ERROR_OK;
}

errorcode_t
syscall_set_handleindex(thread_t* t, handleindex_t* ptr, handleindex_t index)
{
	handleindex_t* p = md_map_thread_memory(t, (void*)ptr, sizeof(handleindex_t), THREAD_MAP_WRITE);
	if (p == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*p = index;
	return ANANAS_ERROR_OK;
}

errorcode_t
syscall_fetch_offset(thread_t* t, const void* ptr, off_t* out)
{
	off_t* o = md_map_thread_memory(t, (void*)ptr, sizeof(off_t), THREAD_MAP_READ);
	if (o == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*out = *o;
	return ANANAS_ERROR_OK;
}

errorcode_t
syscall_set_offset(thread_t* t, void* ptr, off_t len)
{
	off_t* o = md_map_thread_memory(t, (void*)ptr, sizeof(size_t), THREAD_MAP_WRITE);
	if (o == NULL)
		return ANANAS_ERROR(BAD_ADDRESS);

	*o = len;
	return ANANAS_ERROR_OK;
}

/* vim:set ts=2 sw=2: */
