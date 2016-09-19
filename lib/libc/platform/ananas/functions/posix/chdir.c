#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char _posix_cwd[256] = { 0 };

int chdir(const char* path)
{
	errorcode_t err = sys_chdir(path);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	 
	if (path[0] == '/') {
		strncpy(_posix_cwd, path, sizeof(_posix_cwd));
	} else {
		strncat(_posix_cwd, path, sizeof(_posix_cwd) - 1);
	}
	_posix_cwd[sizeof(_posix_cwd) - 1] = '\0';
	return 0;

fail:
	_posix_map_error(err);
	return -1;
}
