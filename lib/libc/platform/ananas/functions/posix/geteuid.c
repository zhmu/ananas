#include <unistd.h>
#include <ananas/procinfo.h>

uid_t
geteuid()
{
	return ananas_procinfo->pi_euid;
}
