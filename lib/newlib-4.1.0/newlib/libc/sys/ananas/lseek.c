/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <ananas/statuscode.h>
#include <errno.h>
#include <unistd.h>
#include "_map_statuscode.h"

off_t lseek(int fd, off_t offset, int whence)
{
    off_t new_offset = offset;
    statuscode_t status = sys_seek(fd, &new_offset, whence);
    if (ananas_statuscode_is_success(status))
        return new_offset;

    return map_statuscode(status);
}
