#include <unistd.h>
#include <fcntl.h>
#include <sys/handle.h>
#include <sys/stat.h>
#include <syscalls.h>

int chdir(const char* path)
{
	void* handle = sys_open(path, O_RDONLY);
	if (handle == NULL)
		return -1;

	return sys_setcwd(handle);
}
