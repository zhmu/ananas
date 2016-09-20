#include <unistd.h>
#include <ananas/procinfo.h>

gid_t
getgid()
{
	return ananas_procinfo->pi_gid;
}
