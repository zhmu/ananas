/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <sys/resource.h>
#include "_map_statuscode.h"

int getrlimit(int resource, struct rlimit* rlp)
{
    statuscode_t status = sys_getrlimit(resource, rlp);
    return map_statuscode(status);
}
