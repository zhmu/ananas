#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int
dup(int fildes)
{
	handleindex_t out;
	statuscode_t status = sys_dupfd(fildes, 0, &out);
	if (status != ananas_statuscode_success())
		return map_statuscode(status);

	return out;
}

/* vim:set ts=2 sw=2: */
