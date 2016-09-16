#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <ananas/thread.h>
#include <sys/wait.h>
#include <_posix/error.h>

pid_t waitpid(pid_t pid, int* stat_loc, int options)
{
	pid_t p = pid;

	errorcode_t err = sys_waitpid(&pid, stat_loc, options);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return (pid_t)-1;
	}
	return (pid_t)p;
}
