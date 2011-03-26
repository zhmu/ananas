#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/error.h>
#include <_posix/fdmap.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h> /* for SEEK_... - XXX is this correct? */

off_t
lseek(int fd, off_t offset, int whence)
{
	errorcode_t err;
	struct HCTL_SEEK_ARG seekarg;
	seekarg.se_offs = &offset;

	switch (whence) {
		case SEEK_SET: seekarg.se_whence = HCTL_SEEK_WHENCE_SET; break;
		case SEEK_CUR: seekarg.se_whence = HCTL_SEEK_WHENCE_CUR; break;
		case SEEK_END: seekarg.se_whence = HCTL_SEEK_WHENCE_END; break;
		default: errno = EINVAL; return -1;
	}

	void* handle = fdmap_deref(fd);
	if (handle == NULL) {
		errno = EBADF;
		return -1;
	}

	err = sys_handlectl(handle, HCTL_FILE_SEEK, &seekarg, sizeof(seekarg));
	if (err == ANANAS_ERROR_NONE)
		return offset;

	_posix_map_error(err);
	return -1;
}
