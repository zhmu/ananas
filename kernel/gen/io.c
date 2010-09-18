#include <machine/param.h>
#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/lib.h>
#include <ananas/handle.h>
#include <ananas/error.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/stat.h>
#include <ananas/schedule.h>
#include <ananas/syscall.h>
#include <ananas/vfs.h>
#include <elf.h>

ssize_t
sys_read(struct HANDLE* handle, void* buf, size_t count)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = syscall_get_file(t, handle);
	if (file == NULL)
		return -1;

	void* buffer = syscall_map_buffer(t, buf, count, THREAD_MAP_WRITE);
	if (buffer == NULL)
		return -1;

	return vfs_read(file, buffer, count);
}

ssize_t
sys_write(struct HANDLE* handle, const void* buf, size_t count)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = syscall_get_file(t, handle);
	if (file == NULL)
		return -1;

	void* buffer = syscall_map_buffer(t, buf, count, THREAD_MAP_READ);
	if (buffer == NULL)
		return -1;

	return vfs_write(file, buffer, count);
}

void*
sys_open(const char* path, int flags)
{
	struct THREAD* t = PCPU_GET(curthread);
	const char* userpath = syscall_map_string(t, path);
	if (userpath == NULL)
		return NULL;

	struct HANDLE* handle = handle_alloc(HANDLE_TYPE_FILE, t);
	if (handle == NULL) {
		thread_set_errorcode(t, ANANAS_ERROR_OUT_OF_HANDLES);
		return NULL;
	}

	if (!vfs_open(userpath, t->path_handle->data.vfs_file.inode, &handle->data.vfs_file)) {
		handle_free(handle);
		thread_set_errorcode(t, ANANAS_ERROR_NO_FILE); /* XXX - more possibilities! */
		return NULL;
	}
	return handle;
}

int
sys_close(void* handle)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct HANDLE* h = syscall_get_handle(t, handle);
	if (h == NULL)
		return -1;

	handle_free(handle);
	return 0;
}

void*
sys_clone(struct HANDLE* handle)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct HANDLE* h = syscall_get_handle(t, handle);
	if (h == NULL)
		return NULL;

	return handle_clone(handle);
}

off_t
sys_seek(struct HANDLE* handle, off_t offset, int whence)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = syscall_get_file(t, handle);
	if (file == NULL)
		return -1;

	kprintf("sys_seek(): todo\n");
	return -1;
}

ssize_t
sys_getdirents(struct HANDLE* handle, void* buf, size_t size)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = syscall_get_file(t, handle);
	if (file == NULL)
		return -1;

	if (!S_ISDIR(file->inode->sb.st_mode)) {
		thread_set_errorcode(t, ANANAS_ERROR_NOT_A_DIRECTORY);
		return -1;
	}

	void* buffer = syscall_map_buffer(t, buf, size, THREAD_MAP_WRITE);
	if (buffer == NULL)
		return -1;

	return vfs_readdir(file, buffer, size);
}

int
sys_setcwd(struct HANDLE* handle)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = syscall_get_file(t, handle);
	if (file == NULL)
		return -1;

	if (!S_ISDIR(file->inode->sb.st_mode)) {
		thread_set_errorcode(t, ANANAS_ERROR_NOT_A_DIRECTORY);
		return -1;
	}

	/* XXX lock */
	handle_free(t->path_handle);
	t->path_handle = handle;
	return 0;
}

int
sys_stat(struct HANDLE* handle, struct stat* sb)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = syscall_get_file(t, handle);
	if (file == NULL) {
		return -1;
	}

	if (file->inode == NULL) {
		return -1; /* XXX device stat support */
	}

	void* buffer = syscall_map_buffer(t, sb, sizeof(struct stat), THREAD_MAP_WRITE);
	if (buffer == NULL) {
		return -1;
	}

	memcpy(buffer, &file->inode->sb, sizeof(struct stat));
	return sizeof(struct stat);
}

handle_event_result_t
sys_wait(void* handle, handle_event_t* event)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct HANDLE* h = syscall_get_handle(t, handle);
	if (h == NULL)
		return -1;

	handle_event_t* e = NULL;
	if (event != NULL) {
		e = syscall_map_buffer(t, event, sizeof(handle_event_t), THREAD_MAP_WRITE);
		if (e == NULL)
			return -1;
	}

	return handle_wait(t, h, e);
}

void*
sys_summon(struct HANDLE* handle, int flags, const char* args)
{
	struct THREAD* t = PCPU_GET(curthread);
	/*
	 * XXX This limits summoning to file-based handles.
	 */
	struct VFS_FILE* file = syscall_get_file(t, handle);
	if (file == NULL)
		return NULL;

	/* Obtain arguments if needed */
	if (args != NULL) {
		args = syscall_map_buffer(t, args, PAGE_SIZE /* XXX */, THREAD_MAP_READ);
		if (args == NULL)
			return NULL;
	}

	/* create a new thread */
	struct THREAD* newthread = thread_alloc(t);
	if (newthread == NULL)
		return NULL;
	if (!elf_load_from_file(newthread, file)) {
		/* failure - no option but to kill the thread */
		thread_set_errorcode(t, ANANAS_ERROR_BAD_EXEC);
		thread_free(newthread);
		thread_destroy(newthread);
		return NULL;
	}

	if (args != NULL)
		thread_set_args(newthread, args);

	thread_resume(newthread); /* XXX - shouldn't this be a flag? */
	return newthread->thread_handle;
}

int sys_remove(struct HANDLE* handle)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = syscall_get_file(t, handle);
	if (file == NULL)
		return -1;

	kprintf("sys_remove(): todo\n");
	return -1;
}

off_t sys_truncate(struct HANDLE* handle, off_t offs)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = syscall_get_file(t, handle);
	if (file == NULL)
		return -1;

	kprintf("sys_truncate(): todo\n");
	return -1;
}

/* vim:set ts=2 sw=2: */
