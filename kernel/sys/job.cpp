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
#include "kernel/processgroup.h"
#include "syscall.h"

Result sys_getpgrp()
{
    Process& proc = thread::GetCurrent().t_process;
    const auto pgid = proc.p_group->pg_id;
    return Result::Success(pgid);
}

Result sys_setpgid(pid_t pid, pid_t pgid)
{
    Process& proc = thread::GetCurrent().t_process;
    if (pid == 0)
        pid = proc.p_pid;
    if (pgid == 0)
        pgid = proc.p_group->pg_id;

    if (pid != proc.p_pid)
        return Result::Failure(EPERM); // XXX for now

    auto process = process_lookup_by_id_and_lock(pid);
    if (process == nullptr)
        return Result::Failure(ESRCH);

    auto pg = process::FindProcessGroupByID(pgid);
    if (!pg) {
        process->Unlock();
        return Result::Failure(ESRCH);
    }

    // XXX pg must be self or of child...

    process::SetProcessGroup(*process, pg);

    process->Unlock();
    pg.Unlock();
    return Result::Success();
}

Result sys_setsid()
{
    Process& proc = thread::GetCurrent().t_process;

    // Reject if we already are a group leader
    if (proc.p_group->pg_session.s_leader == &proc)
        return Result::Failure(EPERM);

    // Exit out current process group and start a new one; this creates a new session.
    // which is what we want
    process::AbandonProcessGroup(proc);
    process::InitializeProcessGroup(proc, nullptr);
    return Result::Success();
}

Result sys_getsid(const pid_t pid)
{
    if (pid != 0)
        return Result::Failure(EPERM); // XXX maybe be more lenient here

    Process& proc = thread::GetCurrent().t_process;
    return Result(proc.p_group->pg_session.s_sid);
}
