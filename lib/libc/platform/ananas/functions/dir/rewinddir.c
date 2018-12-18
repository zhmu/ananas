#include <unistd.h>
#include <dirent.h>

void rewinddir(DIR* dirp)
{
    /* Discard what we have so far */
    dirp->d_cur_pos = 0;

    /* And rewind the stream - we cannot recover on failure as we have no return code */
    (void)lseek(dirp->d_fd, 0, SEEK_SET);
}
