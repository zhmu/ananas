/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/result.h"
#include "syscall.h"

#include "kernel/lib.h"

Result sys_openpt(int flags)
{
    panic("openpt");
}
