/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>

int gethostname(char* name, size_t namelen)
{
    int fd = open("/ankh/network/hostname", O_RDONLY);
    if (fd < 0)
        return -1;

    int n = read(fd, name, namelen);
    if (n >= 0)
        name[n] = '\0';

    close(fd);
    return n >= 0;
}
