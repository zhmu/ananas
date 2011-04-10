#include <unistd.h>
#include <fcntl.h>
#include <_posix/handlemap.h>
#include <_posix/todo.h>

int ftruncate( int fd, off_t length )
{
	void* handle = handlemap_deref(fd, HANDLEMAP_TYPE_FD);
	if (handle == NULL) {
		errno = EBADF;
		return (off_t)-1;
	}

	TODO;
	return -1;
}
