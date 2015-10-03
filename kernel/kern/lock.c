#include <ananas/types.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <machine/interrupts.h>

void
spinlock_lock(spinlock_t* s)
{
	if (scheduler_activated())
		KASSERT(md_interrupts_save(), "interrups must be enabled");

	for(;;) {
		while(atomic_read(&s->sl_var) != 0)
			/* spin */ ;
		if (atomic_xchg(&s->sl_var, 1) == 0)
			break;
	}
}

void
spinlock_unlock(spinlock_t* s)
{
	if (atomic_xchg(&s->sl_var, 0) == 0)
		panic("spinlock %p was not locked", s);
}

void
spinlock_init(spinlock_t* s)
{
	atomic_set(&s->sl_var, 0);
}

register_t
spinlock_lock_unpremptible(spinlock_t* s)
{
	register_t state = md_interrupts_save();

	for(;;) {
		while(atomic_read(&s->sl_var) != 0)
			/* spin */ ;
		md_interrupts_disable();
		if (atomic_xchg(&s->sl_var, 1) == 0)
			break;
		/* Lock failed; restore interrupts and try again */
		md_interrupts_restore(state);
	}
	return state;
}

void
spinlock_unlock_unpremptible(spinlock_t* s, register_t state)
{
	spinlock_unlock(s);
	md_interrupts_restore(state);
}

void
mutex_init(mutex_t* mtx, const char* name)
{
	mtx->mtx_name = name;
	mtx->mtx_owner = NULL;
	mtx->mtx_fname = NULL;
	mtx->mtx_line = 0;
	sem_init(&mtx->mtx_sem, 1);
}

void
mutex_lock_(mutex_t* mtx, const char* fname, int line)
{
	sem_wait(&mtx->mtx_sem);

	/* We got the mutex */
	mtx->mtx_owner = PCPU_GET(curthread);
	mtx->mtx_fname = fname;
	mtx->mtx_line = line;
}

void
mutex_unlock(mutex_t* mtx)
{
	KASSERT(mtx->mtx_owner == PCPU_GET(curthread), "unlocking mutex %p which isn't owned", mtx);
	mtx->mtx_owner = NULL;
	mtx->mtx_fname = NULL;
	mtx->mtx_line = 0;
	sem_signal(&mtx->mtx_sem);
}

void
sem_init(semaphore_t* sem, int count)
{
	KASSERT(count >= 0, "creating semaphore with negative count %d", count);

	spinlock_init(&sem->sem_lock);
	sem->sem_count = count;
	DQUEUE_INIT(&sem->sem_wq);
}

void
sem_signal(semaphore_t* sem)
{
	/*
	 * It is possible that we are run when curthread == NULL; we have to skip the entire
	 * wake-up thing in such a case
	 */
	register_t state = spinlock_lock_unpremptible(&sem->sem_lock);
	thread_t* curthread = PCPU_GET(curthread);
	int wokeup_priority = (curthread != NULL) ? curthread->t_priority : 0;

	if (!DQUEUE_EMPTY(&sem->sem_wq)) {
		/* We have waiters; wake up the first one */
		struct SEMAPHORE_WAITER* sw = DQUEUE_HEAD(&sem->sem_wq);
		DQUEUE_POP_HEAD(&sem->sem_wq);
		sw->sw_signalled = 1;
		thread_resume(sw->sw_thread);
		wokeup_priority = sw->sw_thread->t_priority;
		/* No need to adjust sem_count since the unblocked waiter won't touch it */
	} else {
		/* No waiters; increment the number of units left */
		sem->sem_count++;
	}

	/*
	 * If we woke up something more important than us, mark us as
	 * reschedule.
	 */
	if (curthread != NULL && wokeup_priority < curthread->t_priority)
		curthread->t_flags |= THREAD_FLAG_RESCHEDULE;

	spinlock_unlock_unpremptible(&sem->sem_lock, state);
}	

/* Waits for a semaphore to be signalled, but holds it locked */
static void
sem_wait_and_lock(semaphore_t* sem, register_t* state)
{
	/* Happy flow first: if there are units left, we are done */
	*state = spinlock_lock_unpremptible(&sem->sem_lock);
	if (sem->sem_count > 0) {
		sem->sem_count--;
		return;
	}

	/* No more units; we have to wait */
	thread_t* curthread = PCPU_GET(curthread);
	if (curthread == PCPU_GET(idlethread)) {
		/*
		 * If we are the idle thread, this means we aren't far enough to actually
		 * have other things to schedule - we should just busy-wait instead.
		 *
		 * Note that we need to restore interrupt status as it were; it's likely
		 * an IRQ will actually trigger the wait condition.
		 */
		for (;;) {
			spinlock_unlock_unpremptible(&sem->sem_lock, *state);
			while (sem->sem_count == 0)
				/* wait */;
			spinlock_lock_unpremptible(&sem->sem_lock);
			if (sem->sem_count > 0)
				break;
		}
		sem->sem_count--;
		return;
	}

	/* Non-idle thread: just create a waiter and wait until we get signalled */
	struct SEMAPHORE_WAITER sw;
	sw.sw_thread = curthread;
	sw.sw_signalled = 0;
	DQUEUE_ADD_TAIL(&sem->sem_wq, &sw);
	do {
		thread_suspend(curthread);
		/* Let go of the lock, but keep interrupts disabled */
		spinlock_unlock(&sem->sem_lock);
		schedule();
		spinlock_lock_unpremptible(&sem->sem_lock);
	} while (sw.sw_signalled == 0);
}

void
sem_wait(semaphore_t* sem)
{
	register_t state;
	sem_wait_and_lock(sem, &state);
	spinlock_unlock_unpremptible(&sem->sem_lock, state);
}

void
sem_wait_and_drain(semaphore_t* sem)
{
	register_t state;
	sem_wait_and_lock(sem, &state);
	sem->sem_count = 0; /* drain all remaining units */
	spinlock_unlock_unpremptible(&sem->sem_lock, state);
}

/* vim:set ts=2 sw=2: */
