#include <ananas/types.h>
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/pcpu.h"
#include "kernel/schedule.h"
#include "kernel/thread.h"
#include "kernel-md/interrupts.h"

void
spinlock_lock(Spinlock& s)
{
	if (scheduler_activated())
		KASSERT(md_interrupts_save(), "interrups must be enabled");

	for(;;) {
		while(atomic_read(&s.sl_var) != 0)
			/* spin */ ;
		if (atomic_xchg(&s.sl_var, 1) == 0)
			break;
	}
}

void
spinlock_unlock(Spinlock& s)
{
	if (atomic_xchg(&s.sl_var, 0) == 0)
		panic("spinlock %p was not locked", &s);
}

void
spinlock_init(Spinlock& s)
{
	atomic_set(&s.sl_var, 0);
}

register_t
spinlock_lock_unpremptible(Spinlock& s)
{
	register_t state = md_interrupts_save();

	for(;;) {
		while(atomic_read(&s.sl_var) != 0)
			/* spin */ ;
		md_interrupts_disable();
		if (atomic_xchg(&s.sl_var, 1) == 0)
			break;
		/* Lock failed; restore interrupts and try again */
		md_interrupts_restore(state);
	}
	return state;
}

void
spinlock_unlock_unpremptible(Spinlock& s, register_t state)
{
	spinlock_unlock(s);
	md_interrupts_restore(state);
}

void
mutex_init(Mutex& mtx, const char* name)
{
	mtx.mtx_name = name;
	mtx.mtx_owner = NULL;
	mtx.mtx_fname = NULL;
	mtx.mtx_line = 0;
	sem_init(mtx.mtx_sem, 1);
}

void
mutex_lock_(Mutex& mtx, const char* fname, int line)
{
	sem_wait(mtx.mtx_sem);

	/* We got the mutex */
	mtx.mtx_owner = PCPU_GET(curthread);
	mtx.mtx_fname = fname;
	mtx.mtx_line = line;
}

int
mutex_trylock_(Mutex& mtx, const char* fname, int line)
{
	if (!sem_trywait(mtx.mtx_sem))
		return 0;

	/* We got the mutex */
	mtx.mtx_owner = PCPU_GET(curthread);
	mtx.mtx_fname = fname;
	mtx.mtx_line = line;
	return 1;
}

void
mutex_unlock(Mutex& mtx)
{
	KASSERT(mtx.mtx_owner == PCPU_GET(curthread), "unlocking mutex %p which isn't owned", &mtx);
	mtx.mtx_owner = NULL;
	mtx.mtx_fname = NULL;
	mtx.mtx_line = 0;
	sem_signal(mtx.mtx_sem);
}

void
mutex_assert(Mutex& mtx, int what)
{
	/*
	 * We don't lock the mtx_ fields because the semaphore protects them -
	 * unfortunately, this means this function won't be 100% accurate.
	 */
	switch(what) {
		case MTX_LOCKED:
			if (mtx.mtx_owner != PCPU_GET(curthread) /* not owner */ ||
					mtx.mtx_fname == NULL /* filename not filled out */ ||
					mtx.mtx_line == 0L /* line not filled out */)
				panic("mutex '%s' not held by current thread", mtx.mtx_name);
			break;
		case MTX_UNLOCKED:
			if (mtx.mtx_owner != NULL /* owned by someone */ ||
					mtx.mtx_fname != NULL /* filename filled out */ ||
					mtx.mtx_line != 0L /* line filled out */)
				panic("mutex '%s' held", mtx.mtx_name);
			break;
		default:
			panic("unknown condition %d", what);
	}
}

void
sem_init(Semaphore& sem, int count)
{
	KASSERT(count >= 0, "creating semaphore with negative count %d", count);

	spinlock_init(sem.sem_lock);
	sem.sem_count = count;
	sem.sem_wq.clear();
}

void
sem_signal(Semaphore& sem)
{
	/*
	 * It is possible that we are run when curthread == NULL; we have to skip the entire
	 * wake-up thing in such a case
	 */
	register_t state = spinlock_lock_unpremptible(sem.sem_lock);
	Thread* curthread = PCPU_GET(curthread);
	int wokeup_priority = (curthread != NULL) ? curthread->t_priority : 0;

	if (!sem.sem_wq.empty()) {
		/* We have waiters; wake up the first one */
		SemaphoreWaiter& sw = sem.sem_wq.front();
		sem.sem_wq.pop_front();
		sw.sw_signalled = 1;
		thread_resume(*sw.sw_thread);
		wokeup_priority = sw.sw_thread->t_priority;
		/* No need to adjust sem_count since the unblocked waiter won't touch it */
	} else {
		/* No waiters; increment the number of units left */
		sem.sem_count++;
	}

	/*
	 * If we woke up something more important than us, mark us as
	 * reschedule.
	 */
	if (curthread != NULL && wokeup_priority < curthread->t_priority)
		curthread->t_flags |= THREAD_FLAG_RESCHEDULE;

	spinlock_unlock_unpremptible(sem.sem_lock, state);
}

/* Waits for a semaphore to be signalled, but holds it locked */
static void
sem_wait_and_lock(Semaphore& sem, register_t* state)
{
	/* Happy flow first: if there are units left, we are done */
	*state = spinlock_lock_unpremptible(sem.sem_lock);
	if (sem.sem_count > 0) {
		sem.sem_count--;
		return;
	}

	/*
	 * No more units; we have to wait  - just create a waiter and wait until we
	 * get signalled. We will assume the idle thread never gets here, and
	 * dies in thread_suspend() if we do.
	 */
	Thread* curthread = PCPU_GET(curthread);

	SemaphoreWaiter sw;
	sw.sw_thread = curthread;
	sw.sw_signalled = 0;
	sem.sem_wq.push_back(sw);
	do {
		thread_suspend(*curthread);
		/* Let go of the lock, but keep interrupts disabled */
		spinlock_unlock(sem.sem_lock);
		schedule();
		spinlock_lock_unpremptible(sem.sem_lock);
	} while (sw.sw_signalled == 0);
}

void
sem_wait(Semaphore& sem)
{
	KASSERT(PCPU_GET(nested_irq) == 0, "sem_wait() in irq");

	register_t state;
	sem_wait_and_lock(sem, &state);
	spinlock_unlock_unpremptible(sem.sem_lock, state);
}

void
sem_wait_and_drain(Semaphore& sem)
{
	KASSERT(PCPU_GET(nested_irq) == 0, "sem_wait_and_drain() in irq");

	register_t state;
	sem_wait_and_lock(sem, &state);
	sem.sem_count = 0; /* drain all remaining units */
	spinlock_unlock_unpremptible(sem.sem_lock, state);
}

int
sem_trywait(Semaphore& sem)
{
	int result = 0;

	register_t state = spinlock_lock_unpremptible(sem.sem_lock);
	if (sem.sem_count > 0) {
		sem.sem_count--;
		result++;
	}
	spinlock_unlock_unpremptible(sem.sem_lock, state);

	return result;
}

/* vim:set ts=2 sw=2: */
