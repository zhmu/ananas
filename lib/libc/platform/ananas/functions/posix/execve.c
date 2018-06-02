#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include <assert.h>
#include "_map_statuscode.h"

int execve(const char *path, char *const argv[], char *const envp[])
{
	statuscode_t status = sys_execve(path, (const char**)argv, (const char**)envp);
	/* If we got here, we should have hit an error */
	assert(ananas_statuscode_is_failure(status));
	return map_statuscode(status);
}

/* vim:set ts=2 sw=2: */
