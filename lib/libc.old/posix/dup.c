#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <string.h>
#include <unistd.h>

int
dup(int fildes)
{
	handleindex_t out;
	errorcode_t err = sys_dupfd(fildes, 0, &out);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	return out;
}

/* vim:set ts=2 sw=2: */
