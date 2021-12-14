/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <sys/stat.h>
#include "_map_statuscode.h"

mode_t umask(mode_t cmask)
{
    statuscode_t status = sys_umask(cmask);
    return map_statuscode(status);
}
