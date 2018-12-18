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

int link(const char* oldpath, const char* newpath)
{
    statuscode_t status = sys_link(oldpath, newpath);
    return map_statuscode(status);
}
