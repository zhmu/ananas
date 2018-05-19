#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include "_map_statuscode.h"

int open(const char* filename, int mode, ...)
{
	int perm = 0;
	if (mode & O_CREAT) {
		va_list va;
		va_start(va, mode);
		perm = va_arg(va, unsigned int);
		va_end(va);
	}
	fdindex_t index;
	statuscode_t status = sys_open(filename, mode, perm, &index);
	if (status != ananas_statuscode_success())
		return map_statuscode(status);

	return index;
}
