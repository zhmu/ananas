#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <sys/stat.h>
#include <_posix/error.h>
#include <_posix/fdmap.h>
#include <errno.h>
#include <unistd.h>

int fstat(int fd, struct stat* buf)
{
	errorcode_t err;
	void* handle = fdmap_deref(fd);
	if (handle == NULL) {
		errno = EBADF;
		return -1;
	}

	struct HCTL_STAT_ARG statarg;
	statarg.st_stat_len = sizeof(struct stat);
	statarg.st_stat = buf;
	err = sys_handlectl(handle, HCTL_FILE_STAT, &statarg, sizeof(statarg));
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}
	return 0;
}
