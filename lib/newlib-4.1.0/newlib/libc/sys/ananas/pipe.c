/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <errno.h>
#include <unistd.h>

int pipe(int fildes[2])
{
    errno = ENOSYS;
    return -1;
}
