#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/error.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

off_t
lseek(int fd, off_t offset, int whence)
{
	off_t new_offset = offset;
	errorcode_t err = sys_seek(fd, &new_offset, whence);
	if (err == ANANAS_ERROR_NONE)
		return new_offset;

	_posix_map_error(err);
	return -1;
}
