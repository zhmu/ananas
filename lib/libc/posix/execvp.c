#include <unistd.h>
#include <fcntl.h>
#include <ananas/handle.h>
#include <ananas/stat.h>
#include <syscalls.h>

int execvp(const char *path, char *const argv[])
{
	return execve(path, argv, NULL);
}
