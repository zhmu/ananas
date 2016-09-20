#include <unistd.h>
#include <_posix/todo.h>

int getgroups(int gidsetsize, gid_t grouplist[])
{
	TODO;
	if (gidsetsize > 0)
		grouplist[0] = 0;
	return 1;
}
