#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

int open(const char* filename, int mode, ...)
{
	int perm = 0;
	if (mode & O_CREAT) {
		va_list va;
		va_start(va, mode);
		perm = va_arg(va, unsigned int);
		va_end(va);
	}
	handleindex_t index;
	errorcode_t err = sys_open(filename, mode, perm, &index);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}
	return index;
}
