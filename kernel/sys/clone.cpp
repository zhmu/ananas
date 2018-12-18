/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/syscalls.h>
#include <ananas/errno.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

Result sys_clone(Thread* t, int flags)
{
    TRACE(SYSCALL, FUNC, "t=%p, flags=0x%x", t, flags);
    Process& proc = *t->t_process;

    /* XXX Future improvement so we can do vfork() and such */
    if (flags != 0)
        return RESULT_MAKE_FAILURE(EINVAL);

    /* First, make a copy of the process; this inherits all files and such */
    Process* new_proc;
    if (auto result = proc.Clone(new_proc); result.IsFailure())
        return result;
    // new_proc is locked here and has a single ref

    /* Now clone the handle to the new process */
    Thread* new_thread;
    if (auto result = thread_clone(*new_proc, new_thread); result.IsFailure()) {
        new_proc->RemoveReference(); // destroys it
        return result;
    }
    auto new_pid = new_proc->p_pid;
    new_proc->Unlock();

    /* Resume the cloned thread - it'll have a different return value from ours */
    new_thread->Resume();

    TRACE(SYSCALL, FUNC, "t=%p, success, new pid=%u", t, new_pid);
    return Result::Success(new_pid);
}
