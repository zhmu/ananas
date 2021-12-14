/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <time.h>
#include "_map_statuscode.h"

int setitimer(int which, const struct itimerval* value, struct itimerval* ovalue)
{
    statuscode_t status = sys_setitimer(which, value, ovalue);
    return map_statuscode(status);
}

