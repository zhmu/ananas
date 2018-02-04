#include <ananas/types.h>
#include <ananas/statuscode.h>
#include <ananas/syscalls.h>
#include <errno.h>

#include <stdio.h>

pid_t fork()
{
	pid_t pid;
	statuscode_t status = sys_clone(0, &pid);
	if (status != ananas_statuscode_success()) {
		unsigned int error = ananas_statuscode_extract_errno(status);
		if (error == EEXIST) {
			/* We are the cloned thread - we are all done */
			return 0;
		}

		/* Something did go wrong */
		printf(">>krak[%d->%d,%d]\n", status, error, EEXIST);
		errno = error;
		return -1;
	}

	/* Victory - hand the pid to the parent */
		printf(">>ok[%d]\n", (int)pid);
	return pid;
}
