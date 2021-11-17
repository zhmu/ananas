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

int setpgid(pid_t pid, pid_t pgid)
{
    statuscode_t status = sys_setpgid(pid, pgid);
    return map_statuscode(status);
}
