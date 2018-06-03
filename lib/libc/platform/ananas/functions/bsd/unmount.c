#include <sys/mount.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int
unmount(const char* dir, int flags)
{
	statuscode_t status = sys_unmount(dir, flags);
	return map_statuscode(status);
}
