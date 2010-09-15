#include <unistd.h>
#include <fcntl.h>
#include <sys/handle.h>
#include <sys/stat.h>
#include <syscalls.h>

int fchdir(int fd)
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL)
		return -1;
	return sys_setcwd(handle);
}
