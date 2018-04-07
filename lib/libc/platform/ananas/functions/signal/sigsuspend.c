#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <signal.h>
#include "_map_statuscode.h"

int sigsuspend(const sigset_t* sigmask)
{
	statuscode_t status = sys_sigsuspend(sigmask);
	return map_statuscode(status);
}

