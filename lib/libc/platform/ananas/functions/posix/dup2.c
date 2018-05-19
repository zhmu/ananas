#include <ananas/types.h>
#include <ananas/handle-options.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int
dup2(int fildes, int fildes2)
{
	fdindex_t out = fildes2;
	statuscode_t status = sys_dupfd(fildes, HANDLE_DUPFD_TO, &out);
	if (status != ananas_statuscode_success())
		return map_statuscode(status);

	return out;
}

/* vim:set ts=2 sw=2: */
