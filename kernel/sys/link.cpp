/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/result.h"

Result sys_link(const char* oldpath, const char* newpath)
{
    return Result::Failure(EPERM); // until we implement this
}

Result sys_symlink(const char* oldpath, const char* newpath)
{
    return Result::Failure(EPERM); // until we implement this
}
