#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int pipe(int fildes[2])
{
	struct CREATE_OPTIONS cropts;
	memset(&cropts, 0, sizeof(cropts));
	cropts.cr_size = sizeof(cropts);
	cropts.cr_type = HANDLE_TYPE_PIPE;
	cropts.cr_length = 1024; /* XXX arbitary limit */

	handleindex_t hindex;
	errorcode_t err = sys_create(&cropts, &hindex);
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
	handleindex_t ro_hindex;
	err = sys_clone(hindex, &clopts, &ro_hindex);
	if (err != ANANAS_ERROR_NONE) {
		sys_destroy(hindex);
		_posix_map_error(err);
		return -1;
	}

	/* Create write-only handle */
	handleindex_t wo_hindex;
	clopts.cl_flags = CLONE_FLAG_WRITEONLY;
	err = sys_clone(hindex, &clopts, &wo_hindex);
	if (err != ANANAS_ERROR_NONE) {
		sys_destroy(ro_hindex);
		sys_destroy(hindex);
		_posix_map_error(err);
		return -1;
	}

	/* Throw the original handle away; it has served its purpose */
	sys_destroy(hindex);

	/* Hook them to file descriptors */
	fildes[0] = ro_hindex;
	fildes[1] = wo_hindex;
	return 0;
}
