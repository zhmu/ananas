#include <ananas/types.h>
#include <ananas/thread.h>

#ifndef __SYSCALL_H__
#define __SYSCALL_H__

struct SYSCALL_ARGS {
	register_t	number;
	register_t	arg1, arg2, arg3, arg4, arg5;
};

struct HANDLE;
struct VFS_FILE;

register_t syscall(struct SYSCALL_ARGS* args);

errorcode_t syscall_get_handle(thread_t* t, handle_t handle, struct HANDLE** out);
errorcode_t syscall_get_file(thread_t* t, handle_t handle, struct VFS_FILE** out);
errorcode_t syscall_map_string(thread_t* t, const void* ptr, const char** out);
errorcode_t syscall_map_buffer(thread_t* t, const void* ptr, size_t len, int flags, void** out);
errorcode_t syscall_fetch_size(thread_t* t, const void* ptr, size_t* out);
errorcode_t syscall_set_size(thread_t* t, void* ptr, size_t len);
errorcode_t syscall_set_handle(thread_t* t, handle_t* ptr, handle_t handle);
errorcode_t syscall_fetch_offset(thread_t* t, const void* ptr, off_t* out);
errorcode_t syscall_set_offset(thread_t* t, void* ptr, off_t len);

#endif /* __SYSCALL_H__ */
