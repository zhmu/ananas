#include <fcntl.h>
#include <stdarg.h>
#include <_posix/fdmap.h>

extern void* sys_clone(void* handle);

int fcntl(int fildes, int cmd, ...)
{
	va_list va;
	va_start(va, cmd);

	void* handle = fdmap_deref(fildes);
	if (handle  == NULL)
		return -1;

	switch (cmd) {
		case F_DUPFD: {
			int minfd = va_arg(va, int);
			for (; minfd < FDMAP_SIZE; minfd++) {
				if (fdmap_deref(minfd) != NULL)
					continue;
				void* newhandle = sys_clone(handle);
				if (newhandle == NULL)
					return -1;
				fdmap_set_handle(minfd, newhandle);
				return minfd;
			}
			/* out of descriptors */
			return -1;
		}
		default:
			return -1;
	}
	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
