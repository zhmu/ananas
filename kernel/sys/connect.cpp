/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/_types/socklen.h>
#include "kernel/result.h"
#include "syscall.h"

Result sys_connect(int socket, const struct sockaddr* address, socklen_t address_len)
{
    return Result::Failure(EBADF);
}
