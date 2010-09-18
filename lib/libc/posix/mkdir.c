#include <ananas/stat.h>
#include <fcntl.h>
#include <ananas/handle.h>
#include <ananas/stat.h>
#include <syscalls.h>

int mkdir(const char* path, mode_t mode)
{
	void* handle = sys_open(path, 0 /* XXX */);
	if (handle == NULL)
		return -1;
	sys_close(handle);
	return 0;
}
