#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/result.h"
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
	kprintf("%s: t=%p, sig=%d act=%p oact=%p\n", __func__, t, sig, act, oact);

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

		kprintf("sig %d func %p\n", sig, sact->sa_handler);
	}
	return Result::Success();
}

Result
sys_sigprocmask(Thread* t, int how, const sigset_t* set, sigset_t* oset)
{
	TRACE(SYSCALL, FUNC, "t=%p, how=%d set=%p oset=%p", t, how, set, oset);
	kprintf("%s: t=%p, how=%d set=%p oset=%p\n", __func__, t, how, set, oset);

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
	kprintf("%s: t=%p, pid=%d sig=%d\n", __func__, t, pid, sig);

	// XXX We should support 'sending' signal 0 here...
	return signal::QueueSignal(*t, sig);
}
