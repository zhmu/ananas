/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <sys/resource.h>
#include "_map_statuscode.h"

int setrlimit(int resource, const struct rlimit* rlp)
{
    statuscode_t status = sys_setrlimit(resource, rlp);
    return map_statuscode(status);
}
