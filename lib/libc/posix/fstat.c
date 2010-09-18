#include <unistd.h>
#include <ananas/stat.h>
#include <_posix/fdmap.h>

extern int sys_stat(void* handle, void* buf);

int fstat(int fd, struct stat* buf)
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL)
		return -1;
	return sys_stat(handle, buf);
}
