#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <_posix/fdmap.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int open( const char* filename, int mode, ... )
{
	errorcode_t err;
	void* handle;

	/* XXX we should deal with O_CREAT and O_TRUNC */

	struct OPEN_OPTIONS openopts;
	memset(&openopts, 0, sizeof(openopts));
	openopts.op_size = sizeof(openopts);
	openopts.op_path = filename;
	openopts.op_mode = OPEN_MODE_NONE;
	if (mode & O_RDONLY)
		openopts.op_mode |= OPEN_MODE_READ;
	if (mode & O_WRONLY)
		openopts.op_mode |= OPEN_MODE_WRITE;
	if (mode & O_RDWR)
		openopts.op_mode |= (OPEN_MODE_READ | OPEN_MODE_WRITE);
	if (mode & O_DIRECTORY)
		openopts.op_mode |= OPEN_MODE_DIRECTORY;

	err = sys_open(&openopts, &handle);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}

	int fd = fdmap_alloc_fd(handle);
	if (fd < 0) {
		sys_destroy(handle);
		errno = EMFILE;
		return -1;
	}
	return fd;
}
