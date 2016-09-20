#include <unistd.h>
#include <ananas/procinfo.h>

uid_t
getuid()
{
	return ananas_procinfo->pi_uid;
}
