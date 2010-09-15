#include <unistd.h>
#include <_posix/fdmap.h>

extern void sys_close(void* handle);

int close( int fd )
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL)
		return -1;

	sys_close(handle);
	fdmap_free_fd(fd);
	return 0;
}
