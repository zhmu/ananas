/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"

TRACE_SETUP;

Result sys_rename(Thread* t, const char* oldpath, const char* newpath)
{
    TRACE(SYSCALL, FUNC, "t=%p, oldpath='%s' newpath='%s'", t, oldpath, newpath);
    Process& proc = *t->t_process;
    DEntry* cwd = proc.p_cwd;

    struct VFS_FILE file;
    if (auto result = vfs_open(&proc, oldpath, cwd, &file); result.IsFailure())
        return result;

    auto result = vfs_rename(&file, cwd, newpath);
    vfs_close(&proc, &file);
    return result;
}
