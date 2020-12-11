/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/result.h"

struct Thread;

Result sys_unlink(Thread* t, const char* path)
{
    /* TODO */
    return RESULT_MAKE_FAILURE(EIO);
}
