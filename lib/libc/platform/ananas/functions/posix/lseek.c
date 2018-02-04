#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/statuscode.h>
#include <errno.h>
#include <unistd.h>
#include "_map_statuscode.h"

off_t
lseek(int fd, off_t offset, int whence)
{
	off_t new_offset = offset;
	statuscode_t status = sys_seek(fd, &new_offset, whence);
	if (status == ananas_statuscode_success())
		return new_offset;

	return map_statuscode(status);
}
