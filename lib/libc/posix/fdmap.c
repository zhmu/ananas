#include <unistd.h>
#include <string.h>
#include <_posix/fdmap.h>
#include <sys/threadinfo.h>

void* fd_handle_map[FDMAP_SIZE] = { 0 };

void
fdmap_init(struct THREADINFO* ti)
{
	/* Hook up POSIX standard file handles */
	fd_handle_map[STDIN_FILENO ] = ti->ti_handle_stdin;
	fd_handle_map[STDOUT_FILENO] = ti->ti_handle_stdout;
	fd_handle_map[STDERR_FILENO] = ti->ti_handle_stderr;
}

int
fdmap_alloc_fd(void* handle)
{
	for (unsigned int i = 0; i < FDMAP_SIZE; i++)
		if (fd_handle_map[i] == 0) {
			fd_handle_map[i] = handle;
			return i;
		}
	return -1;
}

void
fdmap_set_handle(int fd, void* handle)
{
	fd_handle_map[fd] = handle;
}

void
fdmap_free_fd(int fd)
{
	fd_handle_map[fd] = 0;
}

void*
fdmap_deref(int fd)
{
	if (fd < 0 || fd >= FDMAP_SIZE)
		return NULL;
	return fd_handle_map[fd];
}

/* vim:set ts=2 sw=2: */
