#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <sys/stat.h>
#include <_posix/error.h>
#include <errno.h>
#include <unistd.h>

int fstat(int fd, struct stat* buf)
{
	errorcode_t err = sys_fstat(fd, buf);
	if (err != ANANAS_ERROR_NONE)
		goto fail;

	return 0;

fail:
	_posix_map_error(err);
	return -1;
}
