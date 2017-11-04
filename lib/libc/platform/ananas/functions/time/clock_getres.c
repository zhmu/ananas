#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <time.h>

int clock_getres(clockid_t id, struct timespec* res)
{
	errorcode_t err = sys_clock_getres(id, res);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	return 0;
}
