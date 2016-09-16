#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

int
closedir(DIR* dirp)
{
	close(dirp->d_fd);
	free(dirp->d_buffer);
	free(dirp);
	return 0;
}
