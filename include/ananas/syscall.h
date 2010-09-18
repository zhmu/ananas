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

struct HANDLE* syscall_get_handle(thread_t t, void* handle);
struct VFS_FILE* syscall_get_file(thread_t t, void* handle);
char* syscall_map_string(thread_t t, const void* ptr);
void* syscall_map_buffer(thread_t t, const void* ptr, size_t len, int flags);

#endif /* __SYSCALL_H__ */
