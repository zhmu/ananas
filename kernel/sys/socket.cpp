/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include <ananas/socket.h>
#include "kernel/result.h"
#include "kernel/net/socket_local.h"
#include "syscall.h"

#include "kernel/lib.h"

Result sys_socket(int domain, int type, int protocol)
{
    if (domain != AF_UNIX || type != SOCK_STREAM || protocol != 0)
        return Result::Failure(EINVAL);

    return net::AllocateLocalSocket();
}
