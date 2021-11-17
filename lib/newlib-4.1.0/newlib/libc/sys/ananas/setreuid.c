/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "_todo.h"

int setreuid(uid_t ruid, uid_t euid)
{
    TODO;
    errno = EPERM;
    return -1;
}
