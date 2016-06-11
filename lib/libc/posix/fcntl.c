#include <ananas/types.h>
#include <ananas/handle-options.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

int fcntl(int fildes, int cmd, ...)
{
	va_list va;
	va_start(va, cmd);

	switch (cmd) {
		case F_DUPFD: {
			int minfd = va_arg(va, int);
			for (; minfd < getdtablesize(); minfd++) {
				if (fcntl(minfd, F_GETFD) >= 0)
					continue;
				return dup2(fildes, minfd);
			}
			/* out of descriptors */
			errno = EMFILE;
			return -1;
		}
		case F_GETFD: {
			struct HCTL_STATUS_ARG sa;
			errorcode_t err = sys_handlectl(fildes, HCTL_GENERIC_STATUS, &sa, sizeof(sa));
			if (err != ANANAS_ERROR_NONE) {
				_posix_map_error(err);
				return -1;
			}
			return sa.sa_flags;
		}
		default:
			errno = EINVAL;
			return -1;
	}
	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
