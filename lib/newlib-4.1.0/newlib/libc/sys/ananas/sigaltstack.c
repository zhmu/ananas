/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <signal.h>
#include "_map_statuscode.h"

int sigaltstack(const stack_t* restrict ss, stack_t* restrict old_ss)
{
    statuscode_t status = sys_sigaltstack(ss, old_ss);
    return map_statuscode(status);
}
