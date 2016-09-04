#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/handlemap.h>
#include <_posix/error.h>
#include <unistd.h>

int fchdir(int fd)
{
	errorcode_t err = sys_fchdir(fd);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	/* XXX what about cwd? */
	return 0;
}
