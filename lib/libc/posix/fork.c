#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <ananas/threadinfo.h>
#include <_posix/error.h>
//#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

pid_t fork()
{
	struct CLONE_OPTIONS clopts;
	memset(&clopts, 0, sizeof(clopts));
	clopts.cl_size = sizeof(clopts);

	/* Now clone the thread itself */
	handleindex_t thread_index;
	errorcode_t err = sys_clone(libc_threadinfo->ti_handle, &clopts, &thread_index);
	if (err != ANANAS_ERROR_NONE) {
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_CLONED) {
			/* We are the cloned thread - we are all done */
			return 0;
		}

fail:
		/* Something did go wrong */
		_posix_map_error(err);
		return -1;
	}

	/* And tell the baby the world is at his feet */
	err = sys_handlectl(thread_index, HCTL_THREAD_RESUME, NULL, 0);
	if (err != ANANAS_ERROR_NONE) {
		sys_destroy(thread_index);
		goto fail;
	}

	/* Victory - hand the pid to the parent */
	return (pid_t)thread_index;
}
