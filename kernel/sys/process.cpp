/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/process.h"
#include "syscall.h"

Result sys_getpid()
{
    Process& proc = thread::GetCurrent().t_process;

    const auto pid = proc.p_pid;
    return Result::Success(pid);
}

Result sys_getppid()
{
    Process& proc = *thread::GetCurrent().t_process.p_parent;

    const auto ppid = proc.p_pid;
    return Result::Success(ppid);
}

Result sys_getuid()
{
    auto uid = 0; // TODO
    return Result::Success(uid);
}

Result sys_geteuid()
{
    auto euid = 0; // TODO
    return Result::Success(euid);
}

Result sys_getgid()
{
    auto gid = 0; // TODO
    return Result::Success(gid);
}

Result sys_getegid()
{
    auto egid = 0; // TODO
    return Result::Success(egid);
}
