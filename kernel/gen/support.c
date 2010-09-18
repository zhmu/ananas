#include <machine/param.h>
#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/handle.h>
#include <ananas/thread.h>

struct HANDLE*
syscall_get_handle(thread_t t, void* handle)
{
	if (!handle_isvalid(handle, t, HANDLE_TYPE_ANY)) {
		thread_set_errorcode(t, ANANAS_ERROR_BAD_HANDLE);
		return NULL;
	}
	return handle;
}

struct VFS_FILE*
syscall_get_file(thread_t t, void* handle)
{
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE)) {
		thread_set_errorcode(t, ANANAS_ERROR_BAD_HANDLE);
		return NULL;
	}

	struct VFS_FILE* file = &((struct HANDLE*)handle)->data.vfs_file;
	if (file->inode == NULL && file->device == NULL) {
		thread_set_errorcode(t, ANANAS_ERROR_BAD_HANDLE);
		return NULL;
	}

	return file;
}

const char*
syscall_map_string(thread_t t, const void* ptr)
{
	char* x = md_map_thread_memory(t, (void*)ptr, PAGE_SIZE, THREAD_MAP_READ);
	/* ensure it's zero terminated */
	for(int i = 0; i < PAGE_SIZE /* XXX */; i++)
		if (x[i] == '\0')
			return x;
	thread_set_errorcode(t, ANANAS_ERROR_BAD_ADDRESS);
	return NULL;
}

void*
syscall_map_buffer(thread_t t, const void* ptr, size_t len, int flags)
{
	void* x = md_map_thread_memory(t, (void*)ptr, len, flags);
	if (x == NULL) {
		thread_set_errorcode(t, ANANAS_ERROR_BAD_ADDRESS);
		return NULL;
	}
	return x;
}

/* vim:set ts=2 sw=2: */
