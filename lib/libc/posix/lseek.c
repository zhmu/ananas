#include <unistd.h>
#include <fcntl.h>
#include <ananas/handle.h>
#include <ananas/stat.h>
#include <syscalls.h>

off_t
lseek(int fd, off_t offset, int whence)
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL)
		return -1;
	return sys_seek(handle, offset, whence);
}
