#include <unistd.h>
#include <_posix/todo.h>

unsigned sleep(unsigned seconds)
{
	TODO;
	return seconds;
}
