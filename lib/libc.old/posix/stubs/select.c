#include <sys/select.h>
#include <_posix/todo.h>

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout)
{
	TODO;
	return -1;
}
