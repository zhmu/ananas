#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <_posix/handlemap.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int
dup(int fildes)
{
	errorcode_t err;

	void* handle = handlemap_deref(fildes, HANDLEMAP_TYPE_ANY);
	if (handle == NULL)  {
		errno = EBADF;
		return -1;
	}

	void* newhandle;
	struct CLONE_OPTIONS clopts;
	memset(&clopts, 0, sizeof(clopts));
	clopts.cl_size = sizeof(clopts);
	err = sys_clone(handle, &clopts, &newhandle);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	int fd = handlemap_alloc_entry(handlemap_get_type(fildes), newhandle);
	if (fd < 0) {
		errno = EMFILE;
		sys_destroy(newhandle);
		return -1;
	}
	return fd;
}

/* vim:set ts=2 sw=2: */
