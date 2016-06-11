#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <string.h>
#include <unistd.h>

int rmdir(const char* path)
{
	errorcode_t err;
	handleindex_t index;

	struct OPEN_OPTIONS openopts;
	memset(&openopts, 0, sizeof(openopts));
	openopts.op_size = sizeof(openopts);
	openopts.op_type = HANDLE_TYPE_FILE;
	openopts.op_path = path;
	openopts.op_mode = OPEN_MODE_DIRECTORY;
	err = sys_open(&openopts, &index);
	if (err != ANANAS_ERROR_NONE)
		goto fail;

	err = sys_unlink(index);
	sys_destroy(index); /* don't really care if this works */
	if (err == ANANAS_ERROR_NONE)
		return 0;

fail:
	_posix_map_error(err);
	return -1;
}
