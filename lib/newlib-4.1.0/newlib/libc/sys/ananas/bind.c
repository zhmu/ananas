/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <sys/socket.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int bind(int socket, const struct sockaddr* address, socklen_t address_len)
{
    statuscode_t status = sys_bind(socket, address, address_len);
    return map_statuscode(status);
}
