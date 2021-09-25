/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <stdlib.h>

int grantpt(int fd)
{
    // For now, don't bother doing anything; we'll create the device on-the-fly
    // with the proper access permissions XXX
    return 0;
}
