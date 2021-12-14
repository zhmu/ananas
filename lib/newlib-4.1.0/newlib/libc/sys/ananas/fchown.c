/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int fchown(int fildes, uid_t owner, gid_t group)
{
    statuscode_t status = sys_fchown(fildes, owner, group);
    return map_statuscode(status);
}
