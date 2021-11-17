/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int stat(const char* path, struct stat* buf)
{
    statuscode_t status = sys_stat(path, buf);
    return map_statuscode(status);
}
