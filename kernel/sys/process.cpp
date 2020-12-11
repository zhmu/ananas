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

Result sys_getpid(Thread* t)
{
    Process& process = t->t_process;

    const auto pid = process.p_pid;
    return Result::Success(pid);
}

Result sys_getppid(Thread* t)
{
    Process& process = *t->t_process.p_parent;

    const auto ppid = process.p_pid;
    return Result::Success(ppid);
}

Result sys_getuid(Thread* t)
{
    auto uid = 0; // TODO
    return Result::Success(uid);
}

Result sys_geteuid(Thread* t)
{
    auto euid = 0; // TODO
    return Result::Success(euid);
}

Result sys_getgid(Thread* t)
{
    auto gid = 0; // TODO
    return Result::Success(gid);
}

Result sys_getegid(Thread* t)
{
    auto egid = 0; // TODO
    return Result::Success(egid);
}
