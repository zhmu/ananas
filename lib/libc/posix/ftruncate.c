#include <unistd.h>
#include <fcntl.h>
#include <ananas/handle.h>
#include <ananas/stat.h>
#include <syscalls.h>
#include <_posix/fdmap.h>

int ftruncate( int fd, off_t length )
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL)
		return (off_t)-1;

	return sys_truncate(handle, length);
}
