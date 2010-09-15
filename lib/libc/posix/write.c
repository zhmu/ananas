#include <unistd.h>
#include <_posix/fdmap.h>

extern ssize_t sys_write(void* handle, const void* buf, size_t len);

ssize_t write(int fd, const void* buf, size_t len)
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL)
		return -1;
	return sys_write(handle, buf, len);
}
