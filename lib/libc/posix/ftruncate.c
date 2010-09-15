#include <unistd.h>
#include <fcntl.h>
#include <sys/handle.h>
#include <sys/stat.h>
#include <syscalls.h>
#include <_posix/fdmap.h>

off_t ftruncate( int fd, off_t length )
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL)
		return (off_t)-1;

	return sys_truncate(handle, length);
}
