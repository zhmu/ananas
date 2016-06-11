#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char _posix_cwd[256] = { 0 };

int chdir(const char* path)
{
	errorcode_t err;
	handleindex_t index;

	struct OPEN_OPTIONS opts;
	memset(&opts, 0, sizeof(opts));
	opts.op_size = sizeof(opts);
	opts.op_type = HANDLE_TYPE_FILE;
	opts.op_path = path;
	opts.op_mode = OPEN_MODE_READ | OPEN_MODE_DIRECTORY;
	err = sys_open(&opts, &index);
	if (err != ANANAS_ERROR_NONE)
		goto fail;
	 
	err = sys_handlectl(index, HCTL_FILE_SETCWD, NULL, 0);
	if (err != ANANAS_ERROR_NONE) {
		sys_destroy(index);
		goto fail;
	}

	if (path[0] == '/') {
		strncpy(_posix_cwd, path, sizeof(_posix_cwd));
	} else {
		strncat(_posix_cwd, path, sizeof(_posix_cwd));
	}
	_posix_cwd[sizeof(_posix_cwd) - 1] = '\0';
	return 0;

fail:
	_posix_map_error(err);
	return -1;
}
