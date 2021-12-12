/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/signal.h>
#include <ananas/errno.h>
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/processgroup.h"
#include "kernel/result.h"
#include "kernel/thread.h"

Result md_core_dump(Thread& t);

namespace signal
{
    enum class DefaultAction {
        TerminateWithCore,
        Terminate,
        Ignore,
        Stop,
        Continue,
    };

    constexpr util::array<DefaultAction, 30>  defaultActionForSignal{
        DefaultAction::Terminate,         // SIGHUP    1
        DefaultAction::Terminate,         // SIGINT    2
        DefaultAction::TerminateWithCore, // SIGQUIT   3
        DefaultAction::TerminateWithCore, // SIGILL    4
        DefaultAction::TerminateWithCore, // SIGTRAP   5
        DefaultAction::TerminateWithCore, // SIGABRT   6
        DefaultAction::TerminateWithCore, // SIGBUS    7
        DefaultAction::TerminateWithCore, // SIGFPE    8
        DefaultAction::Terminate,         // SIGKILL   9
        DefaultAction::Terminate,         // SIGUSR1   10
        DefaultAction::TerminateWithCore, // SIGSEGV   11
        DefaultAction::Terminate,         // SIGUSR2   12
        DefaultAction::Terminate,         // SIGPIPE   13
        DefaultAction::Terminate,         // SIGALRM   14
        DefaultAction::Terminate,         // SIGTERM   15
        DefaultAction::Ignore,            // SIGCHLD   17
        DefaultAction::Continue,          // SIGCONT   18
        DefaultAction::Stop,              // SIGSTOP   19
        DefaultAction::Stop,              // SIGTSTP   20
        DefaultAction::Stop,              // SIGTTIN   21
        DefaultAction::Stop,              // SIGTTOU   22
        DefaultAction::Ignore,            // SIGURG    23
        DefaultAction::TerminateWithCore, // SIGXCPU   24
        DefaultAction::TerminateWithCore, // SIGXFSZ   25
        DefaultAction::TerminateWithCore, // SIGVTALRM 26
        DefaultAction::Terminate,         // SIGPROF   27
        DefaultAction::Ignore,            // SIGWINCH  28
        DefaultAction::Terminate,         // SIGIO     29
        DefaultAction::Terminate,         // SIGPWR    30
        DefaultAction::TerminateWithCore, // SIGSYS    31
    };

    class PendingSignal : public util::List<PendingSignal>::NodePtr
    {
      public:
        PendingSignal(const siginfo_t& si) : ps_SigInfo(si) {}

        const siginfo_t& GetSignalInfo() const { return ps_SigInfo; }

      private:
        siginfo_t ps_SigInfo;
    };

    Result QueueSignal(Thread& t, const siginfo_t& siginfo)
    {
        if (!detail::IsSigalNumberValid(siginfo.si_signo))
            return Result::Failure(EINVAL);

        auto newSignal = new PendingSignal(siginfo);

        {
            auto& tsd = t.t_sigdata;
            SpinlockGuard sg(tsd.tsd_lock);
            tsd.tsd_pending.push_back(*newSignal);
        }

        t.t_flags |= THREAD_FLAG_SIGPENDING; // XXX this needs a lock?
        return Result::Success();
    }

    Result QueueSignal(Thread& t, int signo)
    {
        siginfo_t si{};
        si.si_signo = signo;
        return QueueSignal(t, si);
    }

    Result QueueSignal(process::ProcessGroup& pg, const siginfo_t& si)
    {
        pg.AssertLocked();
        for (auto& p : pg.pg_members) {
            if (p.p_mainthread == nullptr)
                continue; // XXX we should deliver to all threads in the process
            if (auto result = QueueSignal(*p.p_mainthread, si); result.IsFailure())
                return result;
        }

        return Result::Success();
    }

    Action* DequeueSignal(Thread& t, siginfo_t& si)
    {
        auto& tsd = t.t_sigdata;
        SpinlockGuard sg(tsd.tsd_lock);

        /*
         * Note that we need to check signal masks and the like when we are
         * about to _deliver_ a signal, so we could end up finding nothing
         * to deliver here!
         */
        while (!tsd.tsd_pending.empty()) {
            const siginfo_t newSignal = [&] {
                auto& pendingSignal = tsd.tsd_pending.front();
                siginfo_t signalCopy = pendingSignal.GetSignalInfo();
                tsd.tsd_pending.pop_front();
                delete &pendingSignal;
                return signalCopy;
            }();

            int signalNumber = newSignal.si_signo;
            auto sact = tsd.GetSignalAction(signalNumber);
            if (sact->IsIgnored())
                continue;

            // If the signal is masked, do not send it
            if (tsd.tsd_mask.Contains(signalNumber))
                continue;

            si = newSignal;
            return sact;
        }

        // Out of signals - clear pending flag
        t.t_flags &= ~THREAD_FLAG_SIGPENDING; // XXX this needs a lock?
        return nullptr;
    }

    DefaultAction GetSignalDefaultAction(const int signum)
    {
        if (signum <= 0 || signum >= defaultActionForSignal.size())
            return DefaultAction::Ignore;
        return defaultActionForSignal[signum - 1];
    }

    void HandleDefaultSignalAction(const siginfo_t& si)
    {
        auto& curThread = thread::GetCurrent();
        switch (GetSignalDefaultAction(si.si_signo)) {
            case DefaultAction::TerminateWithCore: {
                auto result = md_core_dump(curThread);
                if (result.IsFailure())
                    kprintf("warning: core dump for process %d failed with status code %x\n", curThread.t_process.p_pid, result.AsStatusCode());
                // FALLTHROUGH
            }
            case DefaultAction::Terminate:
                curThread.Terminate(THREAD_MAKE_EXITCODE(THREAD_TERM_SIGNAL, si.si_signo));
                // NOTREACHED
            case DefaultAction::Stop:
                kprintf("TODO: Stop\n");
                return;
            case DefaultAction::Continue:
                kprintf("TODO: Continue\n");
                return;
            case DefaultAction::Ignore:
                return;
        }

        panic("unrecognized default signal action for signal %d", si.si_signo);
        // NOTREACHED
    }

} // namespace signal
