#include <unistd.h>
#include <_posix/todo.h>

char* ttyname(int fildes)
{
	TODO;
	return "console";
}
