#include <unistd.h>
#include <ananas/stat.h>
#include <_posix/fdmap.h>

int lstat(const char* path, struct stat* buf)
{
	/* XXX this needs some work */
	return stat(path, buf);
}
