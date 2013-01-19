#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <_posix/handlemap.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>

int pipe(int fildes[2])
{
	struct CREATE_OPTIONS cropts;
	memset(&cropts, 0, sizeof(cropts));
	cropts.cr_size = sizeof(cropts);
	cropts.cr_type = HANDLE_TYPE_PIPE;
	cropts.cr_length = 1024; /* XXX arbitary limit */

	void* handle;
	errorcode_t err = sys_create(&cropts, &handle);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	/*
	 * Pipes returned have two file handles; the first is read and the
	 * second is write. The semantics are:
	 *
	 * - Closing all pipe write handles must result in reads returning
	 *   zero bytes.
	 * - Closing all pipe read handles and attempting to write something
	 *   should yield a SIGPIPE.
	 *
	 * This works by cloning the handle read/write-only and throwing the
	 * original handle away.
	 */
	struct CLONE_OPTIONS clopts;
	memset(&clopts, 0, sizeof(clopts));
	clopts.cl_size = sizeof(clopts);
	clopts.cl_flags = CLONE_FLAG_READONLY;

	/* Create read-only handle */
	void* rohandle;
	printf(">>> %x\n", clopts.cl_flags);
	err = sys_clone(handle, &clopts, &rohandle);
	if (err != ANANAS_ERROR_NONE) {
		sys_destroy(handle);
		_posix_map_error(err);
		return -1;
	}

	/* Create write-only handle */
	void* wohandle;
	clopts.cl_flags = CLONE_FLAG_WRITEONLY;
	err = sys_clone(handle, &clopts, &wohandle);
	if (err != ANANAS_ERROR_NONE) {
		sys_destroy(rohandle);
		sys_destroy(handle);
		_posix_map_error(err);
		return -1;
	}

	/* Throw the original handle away; it has served its purpose */
	sys_destroy(handle);

	/* Hook them to file descriptors */
	fildes[0] = handlemap_alloc_entry(HANDLEMAP_TYPE_PIPE, rohandle);
	fildes[1] = handlemap_alloc_entry(HANDLEMAP_TYPE_PIPE, wohandle);
	if (fildes[0] < 0 || fildes[1] < 0) {
		sys_destroy(rohandle);
		sys_destroy(wohandle);
		_posix_map_error(err);
		return -1;
	}
	return 0;
}
