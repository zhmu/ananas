#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <time.h>

int clock_gettime(clockid_t id, struct timespec* ts)
{
	errorcode_t err = sys_clock_gettime(id, ts);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	return 0;
}
