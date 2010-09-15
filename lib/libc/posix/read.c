#include <unistd.h>
#include <_posix/fdmap.h>

extern ssize_t sys_read(void* handle, void* buf, size_t len);

ssize_t read(int fd, void* buf, size_t len)
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL)
		return -1;
	return sys_read(handle, buf, len);
}
