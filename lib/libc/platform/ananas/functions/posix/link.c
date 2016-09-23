#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <errno.h>
#include <unistd.h>

int link(const char* oldpath, const char* newpath)
{
	errorcode_t err = sys_link(oldpath, newpath);
	if (err == ANANAS_ERROR_NONE)
		return 0;

	_posix_map_error(err);
	return -1;
}
