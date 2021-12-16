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
#include "kernel/thread.h"
#include "kernel/processgroup.h"
#include "syscall.h"

#include "kernel/lib.h"

namespace
{
    void SanitizeSignalMask(signal::Set& mask)
    {
        mask.Remove(SIGKILL);
        mask.Remove(SIGSTOP);
    }

} // namespace

Result sys_sigaction(const int sig, const struct sigaction* act, struct sigaction* oact)
{
    auto& t = thread::GetCurrent();
    auto& tsd = t.t_sigdata;
    SpinlockGuard sg(tsd.tsd_lock);
    auto sact = tsd.GetSignalAction(sig);
    if (sact == nullptr)
        return Result::Failure(EINVAL);

    if (oact != nullptr)
        *oact = sact->AsSigaction(); // XXX check pointer

    if (act != nullptr) {
        *sact = *act; // XXX check pointer

        // Ensure the signal mask is still proper
        SanitizeSignalMask(sact->sa_mask);
    }
    return Result::Success();
}

Result sys_sigprocmask(const int how, const sigset_t* set, sigset_t* oset)
{
    auto& t = thread::GetCurrent();
    auto& tsd = t.t_sigdata;
    SpinlockGuard sg(tsd.tsd_lock);

    if (oset != nullptr)
        *oset = tsd.tsd_mask.AsSigset(); // XXX verify pointer

    switch (how) {
        case SIG_BLOCK:
            tsd.tsd_mask.Add(*set);
            break;
        case SIG_SETMASK:
            tsd.tsd_mask = *set;
            break;
        case SIG_UNBLOCK:
            tsd.tsd_mask.Remove(*set);
            break;
        default:
            return Result::Failure(EINVAL);
    }
    SanitizeSignalMask(tsd.tsd_mask);
    return Result::Success();
}

Result sys_sigsuspend(const sigset_t* sigmask)
{
    kprintf("%s: TODO, sigmask=%p\n", __func__, sigmask);
    return Result::Failure(EPERM);
}

Result sys_kill(const pid_t pid, const int sig)
{
    auto& proc = process::GetCurrent();
    if (pid == -1) {
        // Send to all processes where we have permission to send to (excluding system processed)
        return Result::Failure(EPERM); // TODO
    }

    siginfo_t si{};
    si.si_signo = sig;
    si.si_pid = proc.p_pid;
    if (pid <= 0) {
        // Send to all processes whose process group ID is equal to ours (pid==0) or |pid| (pid<0)
        int dest_pgid = pid == 0 ? proc.p_group->pg_id : -pid;

        auto pg = process::FindProcessGroupByID(dest_pgid);
        if (!pg)
            return Result::Failure(ESRCH);

        if (sig != 0)
            signal::SendSignal(*pg, si);
        pg.Unlock();
    } else /* pid > 0 */ {
        auto p = process_lookup_by_id_and_lock(pid);
        if (!p)
            return Result::Failure(ESRCH);

        if (sig != 0)
            signal::SendSignal(*p->p_mainthread, si);
        p->Unlock();
    }

    return Result::Success();
}

Result sys_sigaltstack(const stack_t* ss, stack_t* old_ss)
{
    return Result::Failure(EINVAL);
}
