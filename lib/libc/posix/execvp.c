#include <unistd.h>
#include <fcntl.h>
#include <sys/handle.h>
#include <sys/stat.h>
#include <syscalls.h>

int execvp(const char *path, char *const argv[])
{
	return execve(path, argv, NULL);
}
