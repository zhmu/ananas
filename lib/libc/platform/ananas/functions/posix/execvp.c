#include <unistd.h>
#include <stdlib.h>

int execvp(const char *path, char *const argv[])
{
	return execve(path, argv, NULL);
}
