/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <utime.h>
#include "_map_statuscode.h"

int utime(const char* path, const struct utimbuf* times)
{
    statuscode_t status = sys_utime(path, times);
    return map_statuscode(status);
}
