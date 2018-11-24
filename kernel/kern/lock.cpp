#include <ananas/types.h>
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/pcpu.h"
#include "kernel/schedule.h"
#include "kernel/thread.h"
#include "kernel-md/interrupts.h"

namespace {
namespace spinlock {
constexpr int locked = 1;
constexpr int unlocked = 0;
} // namespace spinlock

} // unnamed namespace

void
Spinlock::Lock()
{
	if (scheduler::IsActive())
		KASSERT(md::interrupts::Save(), "interrupts must be enabled");

	while(true) {
		int expected = spinlock::unlocked;
		if (sl_var.compare_exchange_weak(expected, spinlock::locked))
			return;
	}
}

void
Spinlock::Unlock()
{
	if (sl_var.exchange(spinlock::unlocked) != spinlock::locked)
		panic("spinlock %p was not locked", this);
}

Spinlock::Spinlock()
	: sl_var(spinlock::unlocked)
{
}

register_t
Spinlock::LockUnpremptible()
{
	register_t state = md::interrupts::Save();

	for(;;) {
		while(sl_var.load() != spinlock::unlocked)
			/* spin */ ;
		md::interrupts::Disable();
		int expected = spinlock::unlocked;
		if (sl_var.compare_exchange_weak(expected, spinlock::locked))
			break;
		/* Lock failed; restore interrupts and try again */
		md::interrupts::Restore(state);
	}
	return state;
}

void
Spinlock::UnlockUnpremptible(register_t state)
{
	Unlock();
	md::interrupts::Restore(state);
}

Mutex::Mutex(const char* name)
	: mtx_name(name)
{
}

void
Mutex::Lock()
{
	mtx_sem.Wait();
	mtx_owner = PCPU_GET(curthread);
}

bool
Mutex::TryLock()
{
	if (!mtx_sem.TryWait())
		return false;

	mtx_owner = PCPU_GET(curthread);
	return true;
}

void
Mutex::Unlock()
{
	KASSERT(mtx_owner == PCPU_GET(curthread), "unlocking mutex %p which isn't owned", this);
	mtx_owner = nullptr;
	mtx_sem.Signal();
}

void
Mutex::AssertLocked()
{
	if (mtx_owner != PCPU_GET(curthread))
		panic("mutex '%s' not held by current thread", mtx_name);
}

void
Mutex::AssertUnlocked()
{
	if (mtx_owner != nullptr)
		panic("mutex '%s' held", mtx_name);
}

Semaphore::Semaphore(int count)
	: sem_count(count)
{
	KASSERT(count >= 0, "creating semaphore with negative count %d", count);
}

void
Semaphore::Signal()
{
	/*
	 * It is possible that we are run when curthread == NULL; we have to skip the entire
	 * wake-up thing in such a case
	 */
	SpinlockUnpremptibleGuard g(sem_lock);

	Thread* curthread = PCPU_GET(curthread);
	int wokeup_priority = (curthread != nullptr) ? curthread->t_priority : 0;

	if (!sem_wq.empty()) {
		/* We have waiters; wake up the first one */
		SemaphoreWaiter& sw = sem_wq.front();
		sem_wq.pop_front();
		sw.sw_signalled = 1;
		sw.sw_thread->Resume();
		wokeup_priority = sw.sw_thread->t_priority;
		/* No need to adjust sem_count since the unblocked waiter won't touch it */
	} else {
		/* No waiters; increment the number of units left */
		sem_count++;
	}

	/*
	 * If we woke up something more important than us, mark us as
	 * reschedule.
	 */
	if (curthread != nullptr && wokeup_priority < curthread->t_priority)
		curthread->t_flags |= THREAD_FLAG_RESCHEDULE;
}

/* Waits for a semaphore to be signalled, but holds it locked */
register_t
Semaphore::WaitAndLock()
{
	KASSERT(PCPU_GET(nested_irq) == 0, "waiting in irq");

	/* Happy flow first: if there are units left, we are done */
	auto state = sem_lock.LockUnpremptible();
	if (sem_count > 0) {
		sem_count--;
		return state;
	}

	/*
	 * No more units; we have to wait  - just create a waiter and wait until we
	 * get signalled. We will assume the idle thread never gets here, and
	 * dies in Suspend() if we do.
	 */
	Thread* curthread = PCPU_GET(curthread);

	SemaphoreWaiter sw;
	sw.sw_thread = curthread;
	sw.sw_signalled = 0;
	sem_wq.push_back(sw);
	do {
		curthread->Suspend();
		/* Let go of the lock, but keep interrupts disabled */
		sem_lock.Unlock();
		scheduler::Schedule();
		sem_lock.LockUnpremptible();
	} while (sw.sw_signalled == 0);

	return state;
}

void
Semaphore::Wait()
{
	auto state = WaitAndLock();
	sem_lock.UnlockUnpremptible(state);
}

void
Semaphore::WaitAndDrain()
{
	auto state = WaitAndLock();
	sem_count = 0; // drain all remaining units
	sem_lock.UnlockUnpremptible(state);
}

bool
Semaphore::TryWait()
{
	bool result = false;

	auto state = sem_lock.LockUnpremptible();
	if (sem_count > 0) {
		sem_count--;
		result = true;
	}
	sem_lock.UnlockUnpremptible(state);

	return result;
}

/* vim:set ts=2 sw=2: */
