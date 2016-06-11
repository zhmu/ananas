#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

int open(const char* filename, int mode, ...)
{
	struct OPEN_OPTIONS openopts;
	memset(&openopts, 0, sizeof(openopts));
	openopts.op_size = sizeof(openopts);
	openopts.op_type = HANDLE_TYPE_FILE;
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
	if (mode & O_TRUNC)
		openopts.op_mode |= OPEN_MODE_TRUNCATE;
	if (mode & O_CREAT) {
		if (mode & O_EXCL)
			openopts.op_mode |= OPEN_MODE_EXCLUSIVE;
		va_list va;
		va_start(va, mode);
		openopts.op_mode |= OPEN_MODE_CREATE;
		openopts.op_createmode = va_arg(va, unsigned int);
		va_end(va);
	}
	handleindex_t index;
	errorcode_t err = sys_open(&openopts, &index);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}
	return index;
}
