#include <unistd.h>
#include <ananas/procinfo.h>

pid_t
getpid()
{
	return ananas_procinfo->pi_pid;
}
