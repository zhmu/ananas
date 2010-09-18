#include <unistd.h>
#include <ananas/stat.h>
#include <_posix/fdmap.h>

extern void* sys_open(const char* path, int flagS);
extern void  sys_close(void* handle);
extern int sys_stat(void* handle, void* buf);

int stat(const char* path, struct stat* buf)
{
	void* handle = sys_open(path, 0);
	if (handle == NULL)
		return -1;
	int result = sys_stat(handle, buf);
	sys_close(handle);
	return result;
}
