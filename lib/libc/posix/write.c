#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <_posix/fdmap.h>
#include <errno.h>
#include <unistd.h>

ssize_t write(int fd, const void* buf, size_t len)
{
	errorcode_t err;
	void* handle = fdmap_deref(fd);
	if (handle == NULL) {
		errno = EBADF;
		return -1;
	}

	err = sys_write(handle, buf, &len);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}
	return len;
}
