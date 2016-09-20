#include <unistd.h>
#include <ananas/procinfo.h>

pid_t
getppid()
{
	return ananas_procinfo->pi_ppid;
}
