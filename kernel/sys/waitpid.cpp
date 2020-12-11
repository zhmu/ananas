/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/syscalls.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"

Result sys_waitpid(Thread* t, pid_t pid, int* stat_loc, int options)
{
    util::locked<Process> proc;
    if (auto result = t->t_process.WaitAndLock(options, proc); result.IsFailure())
        return result;

    auto child_pid = proc->p_pid;
    if (stat_loc != nullptr)
        *stat_loc = proc->p_exit_status;

    // Give up the parent's reference to the zombie child; this should destroy it
    {
        Process* p = proc.Extract();
        p->RemoveReference();
    }

    return Result::Success(child_pid);
}
