#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/error.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

int execve(const char *path, char *const argv[], char *const envp[])
{
	errorcode_t err = sys_execve(path, (const char**)argv, (const char**)envp);

	/* If we got here, we've hit an error */
	_posix_map_error(err);
	return -1;
}

/* vim:set ts=2 sw=2: */
