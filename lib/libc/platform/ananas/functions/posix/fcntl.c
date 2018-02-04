#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include "_map_statuscode.h"

int fcntl(int fildes, int cmd, ...)
{
	statuscode_t status;
	void* out;

	va_list va;
	va_start(va, cmd);

	switch (cmd) {
		case F_DUPFD:
		case F_SETFD:
		case F_SETFL:
		case F_GETFD:
		case F_GETFL:
			status = sys_fcntl(fildes, cmd, (void*)(uintptr_t)va_arg(va, int), &out);
			if (status != ananas_statuscode_success())
				return map_statuscode(status);

			return (int)(uintptr_t)out;
		default:
			errno = EINVAL;
			return -1;
	}
	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
