#include <ananas/types.h>
#include <ananas/statuscode.h>
#include <ananas/syscalls.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

/* XXX we can get rid of this - we can go from inode -> name */
char _posix_cwd[256] = { 0 };

int chdir(const char* path)
{
	statuscode_t status = sys_chdir(path);
	if (status != ananas_statuscode_success()) {
		errno = ananas_statuscode_extract_errno(status);
		return -1;
	}

	if (path[0] == '/') {
		strncpy(_posix_cwd, path, sizeof(_posix_cwd));
	} else {
		strncat(_posix_cwd, path, sizeof(_posix_cwd) - 1);
	}
	_posix_cwd[sizeof(_posix_cwd) - 1] = '\0';
	return 0;
}
