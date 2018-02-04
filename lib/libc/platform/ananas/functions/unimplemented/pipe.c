#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <errno.h>
#include <unistd.h>

int pipe(int fildes[2])
{
	errno = ENOSYS;
	return -1;
}
