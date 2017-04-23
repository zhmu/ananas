#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/error.h>
#include <sys/wait.h>

pid_t wait(int* stat_loc)
{
	return waitpid(-1, stat_loc, 0 /* TODO look me up */);
}
