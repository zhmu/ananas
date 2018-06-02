#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <sys/wait.h>
#include "_map_statuscode.h"

pid_t waitpid(pid_t pid, int* stat_loc, int options)
{
	statuscode_t status = sys_waitpid(pid, stat_loc, options);
	return map_statuscode(status);
}
