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

int sigprocmask(int how, const sigset_t* set, sigset_t* oset)
{
    statuscode_t status = sys_sigprocmask(how, set, oset);
    return map_statuscode(status);
}
