#include <unistd.h>
#include <ananas/procinfo.h>
#include <_posix/todo.h>

pid_t
getpid()
{
	return ananas_procinfo->pi_pid;
}
