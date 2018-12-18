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
#include "kernel/trace.h"

TRACE_SETUP;

Result sys_utime(Thread* t, const char* path, const struct utimbuf* times)
{
    TRACE(SYSCALL, FUNC, "t=%p, path='%s' times=%p", t, path, times);

    return RESULT_MAKE_FAILURE(EACCES); // TODO: implement me
}
