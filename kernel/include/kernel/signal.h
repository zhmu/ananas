#ifndef __KERNEL_SIGNAL_H__
#define __KERNEL_SIGNAL_H__

#include <ananas/signal.h>
#include <ananas/util/list.h>
#include "kernel/lock.h"

namespace signal
{

namespace detail
{

inline constexpr bool IsSigalNumberValid(int signo)
{
	return signo >= 1 && signo < _SIGLAST;
}

inline constexpr unsigned int GetSignalNumberMask(int signo)
{
	return 1 << (signo - 1);
}

} // namespace detail

// Encapsulates sigset_t - must be in sync with libc's sigset.c
class Set
{
public:
	Set()
	{
		set.sig[0] = 0;
	}

	Set(const sigset_t& ss)
		: set(ss)
	{
	}

	bool Contains(int signo) const
	{
		return (set.sig[0] & detail::GetSignalNumberMask(signo)) != 0;
	}

	void Add(int signo)
	{
		set.sig[0] |= detail::GetSignalNumberMask(signo);
	}

	void Add(const Set& s)
	{
		set.sig[0] |= s.set.sig[0];
	}

	void Remove(int signo)
	{
		set.sig[0] &= ~detail::GetSignalNumberMask(signo);
	}

	void Remove(const Set& s)
	{
		set.sig[0] &= ~s.set.sig[0];
	}

	bool IsEmpty() const
	{
		return set.sig[0] == 0;
	}

	const sigset_t& AsSigset() const
	{
		return set;
	}

private:
	sigset_t set;
};

// Encapsulates sigaction
class Action
{
public:
	Action() = default;
	Action(const sigaction& sa)
		: sa_mask(sa.sa_mask),
		  sa_flags(sa.sa_flags)
	{
		if (sa.sa_flags & SA_SIGINFO)
			sa_handler = reinterpret_cast<Handler>(sa.sa_sigaction);
		else
			sa_handler = reinterpret_cast<Handler>(sa.sa_handler);
	}

	bool IsIgnored() const
	{
		return sa_handler == SIG_IGN;
	}

	sigaction AsSigaction() const
	{
		sigaction sa{};
		sa.sa_mask = sa_mask.AsSigset();
		sa.sa_flags = sa_flags;
		if (sa_flags & SA_SIGINFO)
			sa.sa_sigaction = reinterpret_cast<void(*)(int, siginfo_t*, void*)>(sa_handler);
		else
			sa.sa_handler = sa_handler;
		return sa;
	}

	using Handler = void(*)(int);
	Handler sa_handler = nullptr;
	Set sa_mask;
	int sa_flags = 0;
};

class PendingSignal;

struct ThreadSpecificData
{
	Spinlock		tsd_lock;

	/* Pending signals (waiting to be delivered) */
	util::List<PendingSignal>	tsd_pending;

	/* Masked signals */
	Set	tsd_mask;

	/* Signal handlers */
	Action	tsd_action[_SIGLAST];

	Action* GetSignalAction(int sig)
	{
		if (!detail::IsSigalNumberValid(sig))
			return nullptr;
		return &tsd_action[sig - 1];
	}
};

Result QueueSignal(Thread& t, int signo);
Result QueueSignal(Thread& t, const siginfo_t& siginfo);
Action* DequeueSignal(Thread& t, siginfo_t& si);
void HandleDefaultSignalAction(const siginfo_t& si);

} // namespace signal

#endif /* __KERNEL_SIGNAL_H__ */
