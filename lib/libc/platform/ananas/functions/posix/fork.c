#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>

pid_t fork()
{
	pid_t pid;
	errorcode_t err = sys_clone(0, &pid);
	if (err != ANANAS_ERROR_NONE) {
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_CLONED) {
			/* We are the cloned thread - we are all done */
			return 0;
		}

		/* Something did go wrong */
		_posix_map_error(err);
		return -1;
	}

	/* Victory - hand the pid to the parent */
	return pid;
}
