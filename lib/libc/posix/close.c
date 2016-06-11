#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/error.h>
#include <unistd.h>

int close(int fd)
{
	errorcode_t err = sys_destroy(fd);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}
	return 0;
}
