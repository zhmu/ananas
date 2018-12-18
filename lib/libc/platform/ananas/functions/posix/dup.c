/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include "_map_statuscode.h"

int dup(int fildes)
{
    statuscode_t status = sys_dup(fildes);
    if (ananas_statuscode_is_failure(status))
        return map_statuscode(status);

    return ananas_statuscode_extract_value(status);
}

/* vim:set ts=2 sw=2: */
