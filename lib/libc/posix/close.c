#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/fdmap.h>
#include <_posix/error.h>
#include <errno.h>
#include <unistd.h>

int close( int fd )
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL) {
		errno = EBADF;
		return -1;
	}

	errorcode_t err = sys_destroy(handle);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	fdmap_free_fd(fd);
	return 0;
}
