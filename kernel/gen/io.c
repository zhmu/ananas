#include <machine/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/lib.h>
#include <sys/handle.h>
#include <sys/thread.h>
#include <sys/pcpu.h>
#include <sys/stat.h>
#include <sys/schedule.h>
#include <sys/vfs.h>

extern device_t input_dev;
extern device_t output_dev;

static char*
map_string(thread_t t, void* ptr)
{
	char* x = (char*)md_map_thread_memory(t, ptr, PAGE_SIZE, 0);
	/* ensure it's zero terminated */
	for(int i = 0; i < PAGE_SIZE /* XXX */; i++)
		if (x[i] == '\0')
			return x;
	return NULL;
}

ssize_t
sys_read(struct HANDLE* handle, void* buf, size_t count)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE))
		return -1;

	struct VFS_FILE* file = &handle->data.vfs_file;
	if (file->inode == NULL && file->device == NULL)
		return -1;

	void* x = md_map_thread_memory(t, buf, count, 0);
	if (x == NULL)
		return -1;

	return vfs_read(file, x, count);
}

ssize_t
sys_write(struct HANDLE* handle, const void* buf, size_t count)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE))
		return -1;

	struct VFS_FILE* file = &handle->data.vfs_file;
	if (file->inode == NULL && file->device == NULL)
		return -1;

	void* x = md_map_thread_memory(t, (void*)buf, count, 1);
	if (x == NULL)
		return -1;

	return vfs_write(file, x, count);
}

void*
sys_open(const char* path, int flags)
{
	struct THREAD* t = PCPU_GET(curthread);
	struct HANDLE* handle = handle_alloc(HANDLE_TYPE_FILE, t);
	if (handle == NULL)
		return NULL;

	char* userpath = map_string(t, (void*)path);
	if (userpath == NULL) {
		handle_free(handle);
		return NULL;
	}

	struct VFS_FILE* file = &handle->data.vfs_file;
	if (!vfs_open(userpath, t->path_handle->data.vfs_file.inode, file)) {
		handle_free(handle);
		return NULL;
	}
	return handle;
}

int
sys_close(struct HANDLE* handle)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_ANY))
		return -1;

	handle_free(handle);
	return 0;
}

void*
sys_clone(struct HANDLE* handle)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_ANY))
		return NULL;

	return handle_clone(handle);
}

off_t
sys_seek(struct HANDLE* handle, off_t offset, int whence)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE))
		return -1;

	struct VFS_FILE* file = &handle->data.vfs_file;
	if (file->inode == NULL && file->device == NULL)
		return -1;

	/* TODO */
	return -1;
}

int
sys_getdirents(struct HANDLE* handle, void* buf, size_t size)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE))
		return -1;

	/* XXX check file for directory */
	void* x = md_map_thread_memory(t, (void*)buf, size, 1);
	if (x == NULL)
		return -1;

	struct VFS_FILE* file = &handle->data.vfs_file;
	if (file->inode == NULL && file->device == NULL)
		return -1;

	return vfs_readdir(file, x, size);
}

int
sys_setcwd(struct HANDLE* handle)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE))
		return -1;

	struct VFS_FILE* file = &handle->data.vfs_file;
	if (file->inode == NULL && file->device == NULL)
		return -1;
	if (!S_ISDIR(file->inode->sb.st_mode))
		return -1;

	/* XXX lock */
	handle_free(t->path_handle);
	t->path_handle = handle;
	return 0;
}

int
sys_stat(struct HANDLE* handle, struct stat* sb)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE))
		return -1;

	void* x = md_map_thread_memory(t, (void*)sb, sizeof(struct stat), 1);
	if (x == NULL)
		return -1;

	struct VFS_FILE* file = &handle->data.vfs_file;
	if (file->inode == NULL && file->device == NULL)
		return -1;

	memcpy(x, &file->inode->sb, sizeof(struct stat));
	return 0;
}

handle_event_result_t
sys_wait(void* handle, handle_event_t* event)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_ANY))
		return -1;

	handle_event_t* e = NULL;
	if (event != NULL) {
		e = md_map_thread_memory(t, (void*)event, sizeof(handle_event_t), 1);
		if (e == NULL)
			return -1;
	}

	return handle_wait(t, handle, e);
}

void*
sys_summon(struct HANDLE* handle, int flags, const char* args)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE /* XXX for now */))
		return NULL;

	if (args != NULL) {
		args = map_string(t, (void*)args);
		if (args == NULL)
			return NULL;
	}

	/* create a new thread */
	struct THREAD* newthread = thread_alloc(t);
	if (newthread == NULL)
		return NULL;
	if (!elf_load_from_file(newthread, &handle->data.vfs_file)) {
		/* failure - no option but to kill the thread */
		thread_free(newthread);
		thread_destroy(newthread);
		return NULL;
	}

	if (args != NULL)
		thread_set_args(newthread, args);

	thread_resume(newthread); /* XXX */
	return newthread->thread_handle;
}

int sys_remove(struct HANDLE* handle)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE /* XXX for now */))
		return -1;

	return -1;
}

off_t sys_truncate(struct HANDLE* handle, off_t offs)
{
	struct THREAD* t = PCPU_GET(curthread);
	if (!handle_isvalid(handle, t, HANDLE_TYPE_FILE /* XXX for now */))
		return -1;

	return -1;
}

/* vim:set ts=2 sw=2: */
