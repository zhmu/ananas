/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>

void _exit(int status)
{
    sys_exit(status);
    /* In case we live through this, force an endless loop to avoid 'function may return' error */
    for (;;)
        ;
}
