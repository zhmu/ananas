#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/handle.h>
#include <syscalls.h>

pid_t waitpid(pid_t pid, int *stat_loc, int options)
{
	if (sys_wait((void*)pid, NULL) != 0)
		return pid;
	return (pid_t)-1;
}
