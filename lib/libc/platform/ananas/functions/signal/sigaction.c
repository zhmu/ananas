#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <signal.h>
#include "_map_statuscode.h"

int sigaction(int sig, const struct sigaction* act, struct sigaction* oact)
{
	statuscode_t status = sys_sigaction(sig, act, oact);
	return map_statuscode(status);
}

