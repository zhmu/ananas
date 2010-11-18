#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/fdmap.h>
#include <_posix/error.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int fchdir(int fd)
{
	errorcode_t err;
	void* handle = fdmap_deref(fd);
	if (handle == NULL) {
		errno = EBADF;
		return -1;
	}

	err = sys_handlectl(handle, HCTL_FILE_SETCWD, NULL, 0);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	/* XXX what about cwd? */
	return 0;
}
