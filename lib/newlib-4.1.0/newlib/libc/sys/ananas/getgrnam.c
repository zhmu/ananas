/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/types.h>
#include <grp.h>
#include <errno.h>

struct group* getgrnam(const char* name)
{
    errno = ENOENT;
    return NULL;
}
