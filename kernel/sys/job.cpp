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

Result sys_getpgrp(Thread* t)
{
    Process& process = *t->t_process;

    auto pgid = process.p_group->pg_id;
    return Result::Success(pgid);
}

Result sys_setpgid(Thread* t, pid_t pid, pid_t pgid)
{
    if (pid == 0)
        pid = t->t_process->p_pid;
    if (pgid == 0)
        pgid = t->t_process->p_group->pg_id;

    if (pid != t->t_process->p_pid)
        return RESULT_MAKE_FAILURE(EPERM); // XXX for now

    auto process = process_lookup_by_id_and_lock(pid);
    if (process == nullptr)
        return RESULT_MAKE_FAILURE(ESRCH);

    auto pg = process::FindProcessGroupByID(pgid);
    if (!pg) {
        process->Unlock();
        return RESULT_MAKE_FAILURE(ESRCH);
    }

    // XXX pg must be self or of child...

    process::SetProcessGroup(*process, pg);

    process->Unlock();
    pg.Unlock();
    return Result::Success();
}

Result sys_setsid(Thread* t)
{
    Process& process = *t->t_process;

    // Reject if we already are a group leader
    if (process.p_group->pg_session.s_leader == &process)
        return RESULT_MAKE_FAILURE(EPERM);

    // Exit out current process group and start a new one; this creates a new session.
    // which is what we want
    process::AbandonProcessGroup(process);
    process::InitializeProcessGroup(process, nullptr);
    return Result::Success();
}

Result sys_getsid(Thread* t, pid_t pid)
{
    if (pid != 0)
        return RESULT_MAKE_FAILURE(EPERM); // XXX maybe be more lenient here

    Process& process = *t->t_process;
    return Result(process.p_group->pg_session.s_sid);
}
