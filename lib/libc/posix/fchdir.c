#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/handlemap.h>
#include <_posix/error.h>
#include <unistd.h>

int fchdir(int fd)
{
	errorcode_t err;

	err = sys_handlectl(fd, HCTL_FILE_SETCWD, NULL, 0);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	/* XXX what about cwd? */
	return 0;
}
