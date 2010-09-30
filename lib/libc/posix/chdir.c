#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ananas/handle.h>
#include <ananas/stat.h>
#include <syscalls.h>

char _posix_cwd[256] = { 0 };

int chdir(const char* path)
{
	void* handle = sys_open(path, O_RDONLY);
	if (handle == NULL)
		return -1;
	
	if (sys_setcwd(handle) == 0) {
		if (path[0] == '/') {
			strncpy(_posix_cwd, path, sizeof(_posix_cwd));
		} else {
			strncat(_posix_cwd, path, sizeof(_posix_cwd));
		}
		_posix_cwd[sizeof(_posix_cwd) - 1] = '\0';
		return 0;
	}

	return -1;
}
