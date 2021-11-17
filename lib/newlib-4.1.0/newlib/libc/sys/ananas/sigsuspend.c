/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <signal.h>
#include "_map_statuscode.h"

int sigsuspend(const sigset_t* sigmask)
{
    statuscode_t status = sys_sigsuspend(sigmask);
    return map_statuscode(status);
}
