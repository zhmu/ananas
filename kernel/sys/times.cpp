/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/syscalls.h"

Result sys_times(struct tms* tms)
{
    return Result::Failure(EACCES); // TODO: implement me
}
