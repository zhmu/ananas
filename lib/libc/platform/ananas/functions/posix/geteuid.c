#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

uid_t
geteuid()
{
	statuscode_t status = sys_geteuid();
	return map_statuscode(status);
}
