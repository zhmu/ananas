/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/signal.h>
#include <ananas/errno.h>
#include <ananas/wait.h>
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/processgroup.h"
#include "kernel/result.h"
#include "kernel/thread.h"

Result md_core_dump(Thread& t);
void* md_deliver_signal(struct STACKFRAME* sf, signal::Action* act, siginfo_t& si);

namespace signal
{
    namespace
    {
        enum class DefaultAction {
            TerminateWithCore,
            Terminate,
            Ignore,
            Stop,
            Continue,
        };

        constexpr util::array<DefaultAction, 31>  defaultActionForSignal{
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
            DefaultAction::Terminate,         //           16
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

        Result QueueSignal(Thread& t, const siginfo_t& siginfo)
        {
            if (!detail::IsSigalNumberValid(siginfo.si_signo))
                return Result::Failure(EINVAL);

            {
                auto& tsd = t.t_sigdata;
                SpinlockGuard sg(tsd.tsd_lock);
                tsd.tsd_pending.push(siginfo);
            }

            t.t_sig_pending = 1; // XXX this needs a lock?
            if (t.t_state == thread::State::Suspended) {
                kprintf("todo resume pid %d\n", t.t_process.p_pid);
            }
            return Result::Success();
        }

        DefaultAction GetSignalDefaultAction(const int signum)
        {
            if (signum <= 0 || signum >= defaultActionForSignal.size())
                return DefaultAction::Ignore;
            return defaultActionForSignal[signum - 1];
        }
    }

    Result SendSignal(Thread& t, const siginfo_t& siginfo)
    {
        return QueueSignal(t, siginfo);
    }

    Result SendSignalToParent(Thread& t, const siginfo_t& siginfo)
    {
        auto parent = t.t_process.p_parent->p_mainthread;
        return SendSignal(*parent, siginfo);
    }

    Result SendSignal(process::ProcessGroup& pg, const siginfo_t& si)
    {
        pg.AssertLocked();
        for (auto& p : pg.pg_members) {
            if (p.p_mainthread == nullptr)
                continue; // XXX we should deliver to all threads in the process
            if (auto result = SendSignal(*p.p_mainthread, si); result.IsFailure())
                return result;
        }

        return Result::Success();
    }

    int DequeueSignal(SpinlockGuard&, ThreadSpecificData& tsd, siginfo_t& si)
    {
        /*
         * Note that we need to check signal masks and the like when we are
         * about to _deliver_ a signal, so we could end up finding nothing
         * to deliver here!
         */
        while (!tsd.tsd_pending.empty()) {
            si = tsd.tsd_pending.front();
            tsd.tsd_pending.pop();

            const auto signalNumber = si.si_signo;

            // If the signal is masked, do not send it
            if (tsd.tsd_mask.Contains(signalNumber))
                continue;

            return signalNumber;
        }
        return 0;
    }

    void HandleDefaultSignalAction(const siginfo_t& si)
    {
        auto& curThread = thread::GetCurrent();
        switch (GetSignalDefaultAction(si.si_signo)) {
            case DefaultAction::TerminateWithCore: {
                auto result = md_core_dump(curThread);
                if (result.IsFailure())
                    kprintf("warning: core dump for process %d failed with status code %x\n", curThread.t_process.p_pid, result.AsStatusCode());
                curThread.Terminate(W_MAKE(W_STATUS_SIGNALLED, W_VALUE_COREDUMP | (si.si_signo & 0xff)));
                break;
            }
            case DefaultAction::Terminate:
                curThread.Terminate(W_MAKE(W_STATUS_SIGNALLED, si.si_signo & 0xff));
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

    void* HandleSignal(struct STACKFRAME* sf)
    {
        auto& curThread = thread::GetCurrent();

        while(true) {
            siginfo_t si;
            Action* act;
            int signr;
            {
                auto& tsd = curThread.t_sigdata;
                SpinlockGuard sg(tsd.tsd_lock);
                signr = signal::DequeueSignal(sg, tsd, si);
                if (!signr) {
                    // Out of signals - clear pending flag XXX may need a lock
                    curThread.t_sig_pending = 0;
                    return nullptr;
                }
                act = tsd.GetSignalAction(signr);
            }

            if ((curThread.t_flags & THREAD_FLAG_PTRACED) != 0 && signr != SIGKILL) {
                curThread.t_ptrace_sig = signr;
                curThread.t_state = thread::State::Suspended;

                // Post SIGCHLD to tracer, suspend
                SendSignalToParent(curThread, { .si_signo = SIGCHLD });
                curThread.Suspend();
                scheduler::Schedule();

                // We are back - means the debugger has interfered
                panic("back from suspend");
                if (signr == SIGSTOP)
                    continue;

                signr = curThread.t_ptrace_sig;
                curThread.t_ptrace_sig = 0;
                curThread.t_state = thread::State::Running;
                if (signr == 0)
                    continue; // discard the signal

                si.si_signo = signr;
            }

            if (act->IsIgnored())
                continue;

            if (act->sa_handler == SIG_DFL) {
                signal::HandleDefaultSignalAction(si);
                return nullptr;
            }

            return md_deliver_signal(sf, act, si);
        }
    }
} // namespace signal

extern "C" void* handle_signal(struct STACKFRAME* sf)
{
    return signal::HandleSignal(sf);
}
