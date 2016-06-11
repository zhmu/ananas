#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/error.h>
#include <string.h>
#include <unistd.h>

int access(const char* path, int amode)
{
	struct OPEN_OPTIONS openopts;
	memset(&openopts, 0, sizeof(openopts));
	openopts.op_size = sizeof(openopts);
	openopts.op_type = HANDLE_TYPE_FILE;
	openopts.op_path = path;
	/* F_OK is handled implicely */
	if (amode & R_OK)
		openopts.op_mode |= OPEN_MODE_READ;
	if (amode & W_OK)
		openopts.op_mode |= OPEN_MODE_WRITE;
	/* XXX deal with X_OK */
	handleindex_t index;
	errorcode_t err = sys_open(&openopts, &index);
	if (err == ANANAS_ERROR_OK) {
		sys_destroy(index);
		return 0;
	}
	_posix_map_error(err);
	return -1;
}
