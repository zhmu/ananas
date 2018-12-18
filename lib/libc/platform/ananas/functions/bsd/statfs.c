/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/mount.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int statfs(const char* path, struct statfs* buf)
{
    statuscode_t status = sys_statfs(path, buf);
    return map_statuscode(status);
}
