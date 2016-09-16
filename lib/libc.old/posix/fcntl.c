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
	errorcode_t err;
	void* out;

	va_list va;
	va_start(va, cmd);

	switch (cmd) {
		case F_DUPFD:
		case F_SETFD:
		case F_SETFL:
			err = sys_fcntl(fildes, F_DUPFD, (void*)(uintptr_t)va_arg(va, int), &out);
			if (err != ANANAS_ERROR_NONE)
				goto fail;

			return (int)(uintptr_t)out;
		case F_GETFD:
		case F_GETFL:
			err = sys_fcntl(fildes, F_DUPFD, NULL, &out);
			if (err != ANANAS_ERROR_NONE)
				goto fail;

			return (int)(uintptr_t)out;
		default:
			errno = EINVAL;
			return -1;
	}
	/* NOTREACHED */

fail:
	_posix_map_error(err);
	return -1;
}

/* vim:set ts=2 sw=2: */
