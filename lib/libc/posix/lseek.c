#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <_posix/error.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

off_t
lseek(int fd, off_t offset, int whence)
{
	struct HCTL_SEEK_ARG seekarg;
	seekarg.se_offs = &offset;
	switch (whence) {
		case SEEK_SET: seekarg.se_whence = HCTL_SEEK_WHENCE_SET; break;
		case SEEK_CUR: seekarg.se_whence = HCTL_SEEK_WHENCE_CUR; break;
		case SEEK_END: seekarg.se_whence = HCTL_SEEK_WHENCE_END; break;
		default: errno = EINVAL; return -1;
	}

	errorcode_t err = sys_handlectl(fd, HCTL_FILE_SEEK, &seekarg, sizeof(seekarg));
	if (err == ANANAS_ERROR_NONE)
		return *seekarg.se_offs;

	_posix_map_error(err);
	return -1;
}
