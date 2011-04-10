#include <_posix/handlemap.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

int fcntl(int fildes, int cmd, ...)
{
	va_list va;
	va_start(va, cmd);

	void* handle = handlemap_deref(fildes, HANDLEMAP_TYPE_FD);
	if (handle  == NULL) {
		errno = EBADF;
		return -1;
	}

	switch (cmd) {
		case F_DUPFD: {
			int minfd = va_arg(va, int);
			for (; minfd < HANDLEMAP_SIZE; minfd++) {
				if (handlemap_deref(minfd, HANDLEMAP_TYPE_ANY) != NULL)
					continue;
				return dup2(fildes, minfd);
			}
			/* out of descriptors */
			errno = EMFILE;
			return -1;
		}
		default:
			errno = EINVAL;
			return -1;
	}
	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
