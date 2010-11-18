#include <unistd.h>
#include <fcntl.h>

int execvp(const char *path, char *const argv[])
{
	return execve(path, argv, NULL);
}
