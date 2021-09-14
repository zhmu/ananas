/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <sys/socket.h>
#include "_map_statuscode.h"

ssize_t send(int socket, const void* buffer, size_t length, int flags)
{
    statuscode_t status = sys_send(socket, buffer, length, flags);
    return map_statuscode(status);
}
