#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int rmdir(const char* path)
{
	statuscode_t status = sys_unlink(path);
	return map_statuscode(status);
}
