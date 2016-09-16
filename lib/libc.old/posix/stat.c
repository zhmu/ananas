#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int stat(const char* path, struct stat* buf)
{
	errorcode_t err = sys_stat(path, buf);
	if (err != ANANAS_ERROR_NONE)
		goto fail;

	return 0;

fail:
	_posix_map_error(err);
	return -1;
}
