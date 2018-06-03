#include <sys/mount.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int
mount(const char* type, const char* source, const char* dir, int flags)
{
	statuscode_t status = sys_mount(type, source, dir, flags);
	return map_statuscode(status);
}
