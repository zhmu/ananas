/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

int closedir(DIR* dirp)
{
    close(dirp->d_fd);
    free(dirp->d_buffer);
    free(dirp);
    return 0;
}
