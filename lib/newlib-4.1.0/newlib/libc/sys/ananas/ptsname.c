/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <stdlib.h>
#include <unistd.h>

static char ptsdev[64];

char* ptsname(int fildes)
{
    if (ttyname_r(fildes, ptsdev, sizeof(ptsdev)) != 0)
        return NULL;
    return ptsdev;
}
