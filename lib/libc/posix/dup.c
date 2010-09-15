#include <unistd.h>
#include <_posix/fdmap.h>

extern void* sys_clone(void* handle);
extern void  sys_close(void* handle);

int
dup(int fildes)
{
	void* handle = fdmap_deref(fildes);
	if (handle  == NULL)
		return -1;

	void* newhandle = sys_clone(handle);
	if (newhandle == NULL)
		return -1;

	int fd = fdmap_alloc_fd(newhandle);
	if (fd < 0) {
		sys_close(newhandle);
		return -1;
	}
	return fd;
}

/* vim:set ts=2 sw=2: */
