#include <ananas/types.h>
#include <ananas/stat.h>
#include <ananas/handle.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syscalls.h>

DIR*
opendir(const char* filename)
{
	DIR* dirp;

	int fd = sys_open(filename, O_RDONLY);
	if (fd < 0) {
		errno = ENOENT;
		return NULL;
	}
	/* XXX check for ENODIR */

	dirp = malloc(sizeof(*dirp));
	if (dirp == NULL)
		goto fail;
	dirp->d_fd = fd;
	dirp->d_flags = 0;
	dirp->d_buf_size = DIRENT_BUFFER_SIZE;
	dirp->d_cur_pos = 0;
	dirp->d_buffer = malloc(dirp->d_buf_size);
	if (dirp->d_buffer == NULL)
		goto fail;

	return dirp;


fail:
	if (dirp != NULL) {
		if (dirp->d_buffer != NULL)
			free(dirp->d_buffer);
		free(dirp);
	}
	sys_close(fd);
	return NULL;
}
