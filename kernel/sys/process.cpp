/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/process.h"
#include "syscall.h"

Result sys_getpid()
{
    auto& proc = process::GetCurrent();

    const auto pid = proc.p_pid;
    return Result::Success(pid);
}

Result sys_getppid()
{
    auto& proc = process::GetCurrent();

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

Result sys_setreuid(uid_t ruid, uid_t euid)
{
    return Result::Failure(EPERM);
}

Result sys_setregid(gid_t rgid, gid_t egid)
{
    return Result::Failure(EPERM);
}
