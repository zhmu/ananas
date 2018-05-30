#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/result.h"
#include "kernel/process.h"
#include "kernel/processgroup.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "syscall.h"

#include "kernel/lib.h"

TRACE_SETUP;

namespace
{

void SanitizeSignalMask(signal::Set& mask)
{
	mask.Remove(SIGKILL);
	mask.Remove(SIGSTOP);
}

}

Result
sys_sigaction(Thread* t, int sig, const struct sigaction* act, struct sigaction* oact)
{
	TRACE(SYSCALL, FUNC, "t=%p, sig=%d act=%p oact=%p", t, sig, act, oact);

	auto& tsd = t->t_sigdata;
	SpinlockGuard sg(tsd.tsd_lock);
	auto sact = tsd.GetSignalAction(sig);
	if (sact == nullptr)
		return RESULT_MAKE_FAILURE(EINVAL);

	if (oact != nullptr)
		*oact = sact->AsSigaction(); // XXX check pointer

	if (act != nullptr) {
		*sact = *act; // XXX check pointer

		// Ensure the signal mask is still proper
		SanitizeSignalMask(sact->sa_mask);
	}
	return Result::Success();
}

Result
sys_sigprocmask(Thread* t, int how, const sigset_t* set, sigset_t* oset)
{
	TRACE(SYSCALL, FUNC, "t=%p, how=%d set=%p oset=%p", t, how, set, oset);

	auto& tsd = t->t_sigdata;
	SpinlockGuard sg(tsd.tsd_lock);

	if (oset != nullptr)
		*oset = tsd.tsd_mask.AsSigset(); // XXX verify pointer

	switch(how) {
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
			return RESULT_MAKE_FAILURE(EINVAL);
	}
	SanitizeSignalMask(tsd.tsd_mask);
	return Result::Success();
}

Result
sys_sigsuspend(Thread* t, const sigset_t* sigmask)
{
	TRACE(SYSCALL, FUNC, "t=%p, sigmask=%p", t, sigmask);
	kprintf("%s: TODO t=%p, sigmask=%p\n", __func__, t, sigmask);
	return RESULT_MAKE_FAILURE(EPERM);
}

Result
sys_kill(Thread* t, pid_t pid, int sig)
{
	TRACE(SYSCALL, FUNC, "t=%p, pid=%d sig=%d", t, pid, sig);

	Process& process = *t->t_process;
	if (pid == -1) {
		// Send to all processes where we have permission to send to (excluding system processed)
		return RESULT_MAKE_FAILURE(EPERM); // TODO
	}

	siginfo_t si{};
	si.si_signo = sig;
	si.si_pid = process.p_pid;

	if (pid <= 0) {
		// Send to all processes whose process group ID is equal to ours (pid==0) or |pid| (pid<0)
		int dest_pgid = pid == 0 ? process.p_group->pg_id : -pid;

		auto pg = process::FindProcessGroupByID(dest_pgid);
		if (!pg)
			return RESULT_MAKE_FAILURE(ESRCH);

		if (sig != 0)
			signal::QueueSignal(*pg, si);
		pg.Unlock();
	} else /* pid > 0 */ {
		auto p = process_lookup_by_id_and_lock(pid);
		if (p == nullptr)
			return RESULT_MAKE_FAILURE(ESRCH);

		if (sig != 0)
			signal::QueueSignal(*p->p_mainthread, si);
		p->Unlock();
	}

	return Result::Success();
}
