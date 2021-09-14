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

int socket(int domain, int type, int protocol)
{
    statuscode_t status = sys_socket(domain, type, protocol);
    return map_statuscode(status);
}
