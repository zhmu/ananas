#include <sys/stat.h>

int lstat(const char* path, struct stat* buf)
{
	/* XXX this needs some work */
	return stat(path, buf);
}
