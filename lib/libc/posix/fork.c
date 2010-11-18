#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <ananas/threadinfo.h>
#include <_posix/error.h>
#include <_posix/fdmap.h>
#include <string.h>
#include <unistd.h>

pid_t fork()
{
	struct CLONE_OPTIONS clopts;
	memset(&clopts, 0, sizeof(clopts));
	clopts.cl_size = sizeof(clopts);

	void* newhandle;
	errorcode_t err = sys_clone(libc_threadinfo->ti_handle, &clopts, &newhandle);
	if (err != ANANAS_ERROR_NONE) {
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_CLONED) {
			/* We are the cloned thread */
			return 0;
		}
		/* Something did go wrong */
		_posix_map_error(err);
		return -1;
	}

	/*
	 * XXX this is unfortunate; we should provide a mapping between
	 * handle_t's and pid_t's.
	 */
	return (pid_t)newhandle;
}
