/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <sys/select.h>
#include "kernel/result.h"
#include "syscall.h"

Result sys_select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout)
{
    return Result::Failure(EBADF);
}
