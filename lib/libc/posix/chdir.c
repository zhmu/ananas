#include <unistd.h>
#include <fcntl.h>
#include <ananas/handle.h>
#include <ananas/stat.h>
#include <syscalls.h>

int chdir(const char* path)
{
	void* handle = sys_open(path, O_RDONLY);
	if (handle == NULL)
		return -1;

	return sys_setcwd(handle);
}
