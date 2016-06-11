#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <string.h>
#include <unistd.h>

int
dup(int fildes)
{
	errorcode_t err;

	handleindex_t index;
	struct CLONE_OPTIONS clopts;
	memset(&clopts, 0, sizeof(clopts));
	clopts.cl_size = sizeof(clopts);
	err = sys_clone(fildes, &clopts, &index);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	return index;
}

/* vim:set ts=2 sw=2: */
