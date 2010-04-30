#include <dirent.h>

int
dirfd(DIR* dirp)
{
	return dirp->d_fd;
}
