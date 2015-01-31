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

static ssize_t
pipe_read(int idx, void* handle, void* buf, size_t len)
{
	/* XXX handle NODELAY */
	size_t amount = 0;
	char* ptr = buf;
	while (1) {
		/* Try to read everything we can */
		size_t chunk = len;
		errorcode_t err = sys_read(handle, ptr, &chunk);
		if (err != ANANAS_ERROR_NONE) {
			_posix_map_error(err);
			return -1;
		}
		len -= chunk; ptr += chunk; amount += chunk;

		/* If we read something, return */
		if (chunk > 0)
			break;

		/* Pipe must not have been filled; wait for it to be filled */
		handle_event_t event = HANDLE_EVENT_WRITE;
		handle_event_result_t result;
		err = sys_wait(handle, &event, &result);
		if (err != ANANAS_ERROR_NONE) {
			_posix_map_error(err);
			return -1;
		}
	}
 
	return amount;
}

static ssize_t
pipe_write(int idx, void* handle, const void* buf, size_t len)
{
	/* XXX handle NODELAY */
	size_t amount = 0;
	const char* ptr = buf;
	while (1) {
		/* Try to write everything we can */
		size_t chunk = len;
		errorcode_t err = sys_write(handle, ptr, &chunk);
		if (err != ANANAS_ERROR_NONE) {
			_posix_map_error(err);
			return -1;
		}
		len -= chunk; ptr += chunk; amount += chunk;
		if (len == 0)
			break;

		/* Pipe must not have been emptied; wait for it to be emptied */
		handle_event_t event = HANDLE_EVENT_READ;
		handle_event_result_t result;
		err = sys_wait(handle, &event, &result);
		if (err != ANANAS_ERROR_NONE) {
			_posix_map_error(err);
			return -1;
		}
	} 

	return amount;
}

static struct HANDLEMAP_OPS pipe_ops = {
	.hop_read = pipe_read,
	.hop_write = pipe_write
};

struct HANDLEMAP_OPS* handlemap_ops[] = {
	NULL,
	&fd_ops,
	NULL,
	&pipe_ops
};

/* vim:set ts=2 sw=2: */
