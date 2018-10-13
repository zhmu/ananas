#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

gid_t
getgid()
{
	statuscode_t status = sys_getgid();
	return map_statuscode(status);
}
