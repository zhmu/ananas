/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/result.h"
#include "syscall.h"

Result sys_send(int socket, const void* buffer, size_t length, int flags)
{
    return Result::Failure(EBADF);
}
