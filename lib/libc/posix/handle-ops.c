#include <ananas/threadinfo.h>
#include <ananas/error.h>
#include <ananas/syscalls.h>
#include <_posix/handlemap.h>
#include <_posix/error.h>
#include <errno.h>
#include <stdio.h> /* for SEEK_... - XXX is this correct? */

static ssize_t
fd_read(int idx, void* handle, void* buf, size_t len)
{
	errorcode_t err = sys_read(handle, buf, &len);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}
	return len;
}

static ssize_t
fd_write(int idx, void* handle, const void* buf, size_t len)
{
	errorcode_t err = sys_write(handle, buf, &len);
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}
	return len;
}

static off_t
fd_seek(int idx, void* handle, off_t offset, int whence)
{
	struct HCTL_SEEK_ARG seekarg;
	seekarg.se_offs = &offset;
	switch (whence) {
		case SEEK_SET: seekarg.se_whence = HCTL_SEEK_WHENCE_SET; break;
		case SEEK_CUR: seekarg.se_whence = HCTL_SEEK_WHENCE_CUR; break;
		case SEEK_END: seekarg.se_whence = HCTL_SEEK_WHENCE_END; break;
		default: errno = EINVAL; return -1;
	}

	errorcode_t err = sys_handlectl(handle, HCTL_FILE_SEEK, &seekarg, sizeof(seekarg));
	if (err == ANANAS_ERROR_NONE)
		return offset;

	_posix_map_error(err);
	return -1;
}

static int
fd_stat(int idx, void* handle, struct stat* buf)
{
	struct HCTL_STAT_ARG statarg;
	statarg.st_stat_len = sizeof(struct stat);
	statarg.st_stat = buf;
	errorcode_t err = sys_handlectl(handle, HCTL_FILE_STAT, &statarg, sizeof(statarg));
	if (err != ANANAS_ERROR_NONE) {
		_posix_map_error(err);
		return -1;
	}
	return 0;
}


static struct HANDLEMAP_OPS fd_ops = {
	.hop_read = fd_read,
	.hop_write = fd_write,
	.hop_seek = fd_seek,
	.hop_stat = fd_stat
};

struct HANDLEMAP_OPS* handlemap_ops[] = {
	NULL,
	&fd_ops,
	NULL,
};

/* vim:set ts=2 sw=2: */
