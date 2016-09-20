#include <unistd.h>
#include <ananas/procinfo.h>

gid_t
getegid()
{
	return ananas_procinfo->pi_egid;
}
