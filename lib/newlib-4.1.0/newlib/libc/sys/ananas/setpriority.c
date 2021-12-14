/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <sys/resource.h>
#include "_map_statuscode.h"

int setpriority(int which, id_t who, int value)
{
    statuscode_t status = sys_setpriority(which, who, value);
    return map_statuscode(status);
}
