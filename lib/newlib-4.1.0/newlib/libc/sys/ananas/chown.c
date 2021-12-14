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

int chown(const char* path, uid_t owner, gid_t group)
{
    statuscode_t status = sys_chown(path, owner, group);
    return map_statuscode(status);
}
