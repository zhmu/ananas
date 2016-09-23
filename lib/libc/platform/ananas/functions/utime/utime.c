#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <utime.h>

int utime(const char* path, const struct utimbuf* times)
{
	errorcode_t err = sys_utime(path, times);
	if (err == ANANAS_ERROR_NONE)
		return 0;

	_posix_map_error(err);
	return -1;
}
