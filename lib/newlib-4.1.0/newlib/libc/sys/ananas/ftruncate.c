/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include <fcntl.h>
#include "_map_statuscode.h"

int ftruncate(int fildes, off_t length)
{
    statuscode_t status = sys_ftruncate(fildes, length);
    return map_statuscode(status);
}
