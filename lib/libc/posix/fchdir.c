#include <unistd.h>
#include <fcntl.h>
#include <ananas/handle.h>
#include <ananas/stat.h>
#include <syscalls.h>

int fchdir(int fd)
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL)
		return -1;
	/* XXX what about cwd? */
	return sys_setcwd(handle);
}
