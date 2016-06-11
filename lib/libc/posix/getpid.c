#include <unistd.h>
#include <ananas/threadinfo.h>
#include <_posix/todo.h>

pid_t
getpid()
{
	TODO;
	return (pid_t)-1;
}
