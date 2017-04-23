#include <ananas/types.h>
#include <ananas/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

DIR*
opendir(const char* filename)
{
	DIR* dirp;

	int fd = open(filename, O_RDONLY | O_DIRECTORY);
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
	close(fd);
	return NULL;
}
