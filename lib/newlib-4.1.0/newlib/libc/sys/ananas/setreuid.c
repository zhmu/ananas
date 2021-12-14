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

int setreuid(uid_t ruid, uid_t euid)
{
    statuscode_t status = sys_setreuid(ruid, euid);
    return map_statuscode(status);
}
