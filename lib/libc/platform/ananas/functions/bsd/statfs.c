#include <sys/mount.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int
statfs(const char* path, struct statfs* buf)
{
	statuscode_t status = sys_statfs(path, buf);
	return map_statuscode(status);
}
