/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <ananas/syscalls.h>
#include "_map_statuscode.h"

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout)
{
    statuscode_t status = sys_select(nfds, readfds, writefds, errorfds, timeout);
    return map_statuscode(status);
}
