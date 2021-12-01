/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <sys/times.h>
#include "_map_statuscode.h"

clock_t times(struct tms* buffer)
{
    statuscode_t status = sys_times(buffer);
    return map_statuscode(status);
}
