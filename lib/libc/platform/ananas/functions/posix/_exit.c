#include <ananas/types.h>
#include <ananas/handle-options.h>
#include <ananas/syscall-vmops.h>
#include <_gen/syscalls.h>

void
_exit(int status)
{
	sys_exit(status);
	/* In case we live through this, force an endless loop to avoid 'function may return' error */
	for(;;);
}
