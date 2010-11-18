#include <unistd.h>
#include <fcntl.h>
#include <_posix/fdmap.h>
#include <_posix/todo.h>

int ftruncate( int fd, off_t length )
{
	void* handle = fdmap_deref(fd);
	if (handle == NULL) {
		errno = EBADF;
		return (off_t)-1;
	}

	TODO;
	return -1;
}
