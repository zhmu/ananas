#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <_posix/handlemap.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int
dup2(int fildes, int fildes2)
{
	errorcode_t err;

	void* handle = handlemap_deref(fildes, HANDLEMAP_TYPE_ANY);
	if (handle == NULL) {
		errno = EBADF;
		return -1;
	}
	if (fildes2 < 0 || fildes2 >= HANDLEMAP_SIZE) {
		errno = EBADF;
		return -1;
	}

	if (handlemap_deref(fildes2, HANDLEMAP_TYPE_ANY) != NULL)
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

	/* We've eliminated the previous descriptor, so we'll need to make a new one */
	handlemap_set_entry(fildes2, handlemap_get_type(fildes), newhandle);
	return fildes2;
}

/* vim:set ts=2 sw=2: */
