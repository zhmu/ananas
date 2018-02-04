#include <ananas/types.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int fchdir(int fd)
{
	statuscode_t status = sys_fchdir(fd);
	return map_statuscode(status);
}
