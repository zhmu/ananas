#include <unistd.h>
#include <sys/mount.h>
#include <_posix/todo.h>

int statfs(const char *path, struct statfs *buf)
{
	TODO;
	return -1;
}
