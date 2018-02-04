#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <sys/wait.h>
#include "_map_statuscode.h"

pid_t waitpid(pid_t pid, int* stat_loc, int options)
{
	pid_t p = pid;

	statuscode_t status = sys_waitpid(&p, stat_loc, options);
	if (status != ananas_statuscode_success()) {
		return (pid_t)map_statuscode(status);
	}
	return p;
}
