/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/syscalls.h>
#include "kernel/result.h"

Result sys_utime(Thread* t, const char* path, const struct utimbuf* times)
{
    return Result::Failure(EACCES); // TODO: implement me
}
