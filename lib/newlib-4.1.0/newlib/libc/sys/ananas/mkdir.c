/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <fcntl.h>
#include "_map_statuscode.h"

int mkdir(const char* path, mode_t mode)
{
    statuscode_t status = sys_mkdir(path, mode);
    return map_statuscode(status);
}
