#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <string.h>
#include <unistd.h>

int unlink(const char* path)
{
	errorcode_t err = sys_unlink(path);
	if (err == ANANAS_ERROR_NONE)
		return 0;

	_posix_map_error(err);
	return -1;
}
