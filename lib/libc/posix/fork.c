#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <ananas/threadinfo.h>
#include <_posix/error.h>
#include <_posix/handlemap.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

pid_t fork()
{
	struct CLONE_OPTIONS clopts;
	memset(&clopts, 0, sizeof(clopts));
	clopts.cl_size = sizeof(clopts);

	/* Preallocate a pid entry; we need it if the fork succeeds */
	int pid = handlemap_alloc_entry(HANDLEMAP_TYPE_PID, NULL);
	if (pid < 0) {
		errno = EMFILE;
		return -1;
	}

	void* newhandle;
	errorcode_t err = sys_clone(libc_threadinfo->ti_handle, &clopts, &newhandle);
	if (err != ANANAS_ERROR_NONE) {
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_CLONED) {
			/* We are the cloned thread */
			return 0;
		}
		/* Something did go wrong */
		handlemap_free_entry(pid);
		_posix_map_error(err);
		return -1;
	}

	handlemap_set_handle(pid, newhandle);
	return (pid_t)pid;
}
