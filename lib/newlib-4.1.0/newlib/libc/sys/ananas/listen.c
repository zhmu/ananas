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

int listen(int socket, int backlog)
{
    statuscode_t status = sys_listen(socket, backlog);
    return map_statuscode(status);
}
