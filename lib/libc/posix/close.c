#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/handlemap.h>
#include <_posix/error.h>
#include <errno.h>
#include <unistd.h>

int close(int fd)
{
	void* handle = handlemap_deref(fd, HANDLEMAP_TYPE_FD);
	if (handle == NULL) {
		errno = EBADF;
		return -1;
	}

	errorcode_t err = sys_destroy(handle);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	handlemap_free_entry(fd);
	return 0;
}
