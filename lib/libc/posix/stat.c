#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int stat(const char* path, struct stat* buf)
{
	errorcode_t err;
	handleindex_t index;

	struct OPEN_OPTIONS openopts;
	memset(&openopts, 0, sizeof(openopts));
	openopts.op_size = sizeof(openopts);
	openopts.op_type = HANDLE_TYPE_FILE;
	openopts.op_path = path;
	openopts.op_mode = OPEN_MODE_NONE;
	err = sys_open(&openopts, &index);
	if (err != ANANAS_ERROR_NONE)
		goto fail;

	struct HCTL_STAT_ARG statarg;
	statarg.st_stat_len = sizeof(struct stat);
	statarg.st_stat = buf;
	err = sys_handlectl(index, HCTL_FILE_STAT, &statarg, sizeof(statarg));
	sys_destroy(index);
	if (err != ANANAS_ERROR_NONE)
		goto fail;

	return 0;

fail:
	_posix_map_error(err);
	return -1;
}
