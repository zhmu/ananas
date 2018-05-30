#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

pid_t getpgrp()
{
	pid_t pgid;
	statuscode_t status = sys_getpgrp(&pgid);
	if (status == ananas_statuscode_success())
		return pgid;

	return map_statuscode(status);
}
