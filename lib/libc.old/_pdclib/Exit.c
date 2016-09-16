#include <ananas/syscalls.h>

void
_PDCLIB_Exit( int status )
{
	sys_exit(status);
}
