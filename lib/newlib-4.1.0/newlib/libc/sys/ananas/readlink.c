/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

ssize_t readlink(const char* path, char* buffer, size_t bufsize)
{
    statuscode_t status = sys_readlink(path, buffer, bufsize);
    return map_statuscode(status);
}
