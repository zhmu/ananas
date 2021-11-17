/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include "_map_statuscode.h"

int _open(const char* filename, int mode, int perm)
{
    statuscode_t status = sys_open(filename, mode, perm);
    return map_statuscode(status);
}
