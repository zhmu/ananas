#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

pid_t getpgrp()
{
	statuscode_t status = sys_getpgrp();
	return map_statuscode(status);
}
