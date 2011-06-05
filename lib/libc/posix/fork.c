#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <ananas/threadinfo.h>
#include <_posix/error.h>
#include <_posix/handlemap.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define NUM_RESERVED_HANDLES (STDERR_FILENO + 1)

pid_t fork()
{
	struct CLONE_OPTIONS clopts;
	memset(&clopts, 0, sizeof(clopts));
	clopts.cl_size = sizeof(clopts);

	/*
	 * Okay, this royally sucks: we'll have to pass all current handles to the
	 * cloned thread. To do this, we'll just clone everything that isn't a
	 * standard handle (the kernel will handle these as they are part of
	 * the threadinfo structure). If the clone fails, we'll just throw them
	 * away.
	 */
	void* cloned_handles[HANDLEMAP_SIZE];
	memset(cloned_handles, 0, sizeof(void*) * HANDLEMAP_SIZE);
	for (int i = NUM_RESERVED_HANDLES; i < HANDLEMAP_SIZE; i++) {
		void* handle = handlemap_deref(i, HANDLEMAP_TYPE_ANY);
		if (handle == NULL)
			continue;
		void* newhandle;
		errorcode_t err = sys_clone(handle, &clopts, &newhandle);
		if (err != ANANAS_ERROR_OK)
			goto fail;
		cloned_handles[i] = newhandle;
	}

	/* Preallocate a pid entry; we need it if the fork succeeds */
	int pid = handlemap_alloc_entry(HANDLEMAP_TYPE_PID, NULL);
	if (pid < 0) {
		errno = EMFILE;
		return -1;
	}

	/* Now clone the thread itself */
	void* newthread;
	errorcode_t err = sys_clone(libc_threadinfo->ti_handle, &clopts, &newthread);
	if (err != ANANAS_ERROR_NONE) {
		/* Throw away the pid entry; the cloned thread doesn't need it */
		handlemap_free_entry(pid);
		if (ANANAS_ERROR_CODE(err) == ANANAS_ERROR_CLONED) {
			/*
			 * We are the cloned thread - reinitialize our standard
			 * handles; we need to use the copies instead of our
			 * parents.
			 */
			handlemap_reinit(libc_threadinfo);

			/*
			 * Restore our cloned handles - they will already be
 			 * given to us by our parent
			 */
			for (int i = NUM_RESERVED_HANDLES; i < HANDLEMAP_SIZE; i++) {
				if (cloned_handles[i] != NULL)
					handlemap_set_handle(i, cloned_handles[i]);
			}
			return 0;
		}

fail:
		/* Something did go wrong */
		_posix_map_error(err);

		/* Throw away the cloned handles; they won't be needed anymore */
		for (int i = NUM_RESERVED_HANDLES; i < HANDLEMAP_SIZE; i++) {
			if (cloned_handles[i] != NULL)
				sys_destroy(cloned_handles[i]);
		}
		return -1;
	}

	/* Give all our handles to our baby */
	for (int i = NUM_RESERVED_HANDLES; i < HANDLEMAP_SIZE; i++) {
		if (cloned_handles[i] == NULL)
			continue;
		err = sys_handlectl(cloned_handles[i], HCTL_GENERIC_SETOWNER, newthread, sizeof(void*));
		if (err != ANANAS_ERROR_NONE) {
			sys_destroy(newthread);
			goto fail;
		}
	}

	/* And tell the baby the world is at his feet */
	err = sys_handlectl(newthread, HCTL_THREAD_RESUME, NULL, 0);
	if (err != ANANAS_ERROR_NONE) {
		sys_destroy(newthread);
		goto fail;
	}

	/* Victory - hand the pid to the parent */
	handlemap_set_handle(pid, newthread);
	return (pid_t)pid;
}
