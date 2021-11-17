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

int setregid(gid_t rgid, gid_t egid)
{
    TODO;
    errno = EPERM;
    return -1;
}
