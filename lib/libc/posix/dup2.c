#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <_posix/fdmap.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int
dup2(int fildes, int fildes2)
{
	errorcode_t err;

	void* handle = fdmap_deref(fildes);
	if (handle == NULL) {
		errno = EBADF;
		return -1;
	}
	if (fildes2 < 0 || fildes2 >= FDMAP_SIZE) {
		errno = EBADF;
		return -1;
	}

	if (fdmap_deref(fildes2) != NULL)
		close(fildes2);

	void* newhandle;
	struct CLONE_OPTIONS clopts;
	memset(&clopts, 0, sizeof(clopts));
	clopts.cl_size = sizeof(clopts);
	err = sys_clone(handle, &clopts, &newhandle);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	fdmap_set_handle(fildes2, newhandle);
	return 0;
}

/* vim:set ts=2 sw=2: */
