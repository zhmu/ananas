/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <grp.h>
#include "_map_statuscode.h"

int setgroups(int gidsetsize, const gid_t* list)
{
    statuscode_t status = sys_setgroups(gidsetsize, list);
    return map_statuscode(status);
}
