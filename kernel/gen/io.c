#include <machine/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/lib.h>
#include <sys/thread.h>
#include <sys/pcpu.h>
#include <sys/vfs.h>

extern device_t input_dev;
extern device_t output_dev;

static int
find_avail_fd(thread_t t)
{
	for(unsigned int i = 0; i < THREAD_MAX_FILES; i++) {
		if (t->file[i].device == NULL && t->file[i].inode == NULL)
			return i;
	}
	return -1;
}

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
sys_read(int fd, void* buf, size_t count)
{
	if (fd < 0 || fd >= THREAD_MAX_FILES)
		return -1;
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = &t->file[fd];
	if (file->inode == NULL && file->device == NULL)
		return -1;

	void* x = md_map_thread_memory(PCPU_GET(curthread), buf, count, 0);
	if (x == NULL)
		return -1;

	return vfs_read(file, x, count);
}

ssize_t
sys_write(int fd, const void* buf, size_t count)
{
	if (fd < 0 || fd >= THREAD_MAX_FILES)
		return -1;
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = &t->file[fd];
	if (file->inode == NULL && file->device == NULL)
		return -1;

	void* x = md_map_thread_memory(t, (void*)buf, count, 1);
	if (x == NULL)
		return -1;

	return vfs_write(file, x, count);
}

int
sys_open(const char* path, int flags)
{
	struct THREAD* t = PCPU_GET(curthread);

	int fd = find_avail_fd(t);
	if (fd < 0)
		return -1;

	char* userpath = map_string(t, (void*)path);
	if (userpath == NULL)
		return -1;

	if (!vfs_open(userpath, &t->file[fd]))
		return -1;
	return fd;
}

void
sys_close(int fd)
{
	if (fd < 0 || fd >= THREAD_MAX_FILES)
		return;

	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = &t->file[fd];
	vfs_close(file);
}

off_t
sys_lseek(int fd, off_t offset, int whence)
{
	if (fd < 0 || fd >= THREAD_MAX_FILES)
		return -1;

	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = &t->file[fd];
	if (file->inode == NULL)
		return 0;

	/* TODO */
	return -1;
}

int
sys_getdirents(int fd, void* buf, size_t numitems)
{
	if (fd < 0 || fd >= THREAD_MAX_FILES)
		return -1;
	struct THREAD* t = PCPU_GET(curthread);
	struct VFS_FILE* file = &t->file[fd];
	if (file->inode == NULL)
		return -1;

	void* x = md_map_thread_memory(t, (void*)buf, sizeof(struct VFS_DIRENT) * numitems, 1);
	if (x == NULL)
		return -1;
	return vfs_readdir(file, x, numitems);
}

/* vim:set ts=2 sw=2: */
