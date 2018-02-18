#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int chdir(const char* path)
{
	statuscode_t status = sys_chdir(path);
	return map_statuscode(status);
}
