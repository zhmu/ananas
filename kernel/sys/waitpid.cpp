#include <ananas/syscalls.h>
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"

TRACE_SETUP;

Result sys_waitpid(Thread* t, pid_t pid, int* stat_loc, int options)
{
    TRACE(SYSCALL, FUNC, "t=%p, pid=%d stat_loc=%p options=%d", t, pid, stat_loc, options);

    util::locked<Process> proc;
    if (auto result = t->t_process->WaitAndLock(options, proc); result.IsFailure())
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

/* vim:set ts=2 sw=2: */
