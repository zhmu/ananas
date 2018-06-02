#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

ssize_t read(int fd, void* buf, size_t len)
{
	statuscode_t status = sys_read(fd, buf, len);
	return map_statuscode(status);
}
