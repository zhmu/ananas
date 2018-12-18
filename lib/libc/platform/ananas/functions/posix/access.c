/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int access(const char* path, int amode)
{
    struct stat sb;
    if (stat(path, &sb) < 0)
        return -1; /* already sets errno */

    /* XXX This is wrong - we should let the kernel do it */
    if ((amode & X_OK) && (sb.st_mode & 0111) == 0)
        goto no_access;
    if ((amode & R_OK) && (sb.st_mode & 0222) == 0)
        goto no_access;
    if ((amode & W_OK) && (sb.st_mode & 0444) == 0)
        goto no_access;

    return 0;

no_access:
    errno = EACCES;
    return -1;
}
