#include <ananas/types.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/waitqueue.h>
#include <machine/atomic.h>
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
	atomic_set(&mtx->mtx_var, 0);
	waitqueue_init(&mtx->mtx_wq);
}

void
mutex_lock_(mutex_t* mtx, const char* fname, int line)
{
	for(;;) {
		while(atomic_read(&mtx->mtx_var) != 0) {
			/* Not owned; wait for it */
			struct WAITER* w = waitqueue_add(&mtx->mtx_wq);
			waitqueue_wait(w);
			waitqueue_remove(w);
		}
		if (atomic_xchg(&mtx->mtx_var, 1) == 0)
			break;
	}

	/* We got the mutex */
	mtx->mtx_owner = PCPU_GET(curthread);
#ifdef MUTEX_DEBUG
	mtx->mtx_fname = fname;
	mtx->mtx_line = line;
#endif
}

void
mutex_unlock(mutex_t* mtx)
{
	KASSERT(mtx->mtx_owner == PCPU_GET(curthread), "unlocking mutex %p which isn't owned", mtx);
	if (atomic_xchg(&mtx->mtx_var, 0) == 0)
		panic("mutex %p was not locked", mtx); /* should be very unlikely */
	mtx->mtx_owner = NULL;
#ifdef MUTEX_DEBUG
	mtx->mtx_fname = NULL;
	mtx->mtx_line = 0;
#endif
	waitqueue_signal(&mtx->mtx_wq);
}

void
sem_init(semaphore_t* sem, int count)
{
	spinlock_init(&sem->sem_lock);
	waitqueue_init(&sem->sem_wq);
	sem->sem_count = count;
}

void
sem_signal(semaphore_t* sem)
{
	/* Increment the number of units left */
	register_t state = spinlock_lock_unpremptible(&sem->sem_lock);
	sem->sem_count++;
	spinlock_unlock_unpremptible(&sem->sem_lock, state);

	/* Wake up anyone who is waiting on this semaphore */
	waitqueue_signal(&sem->sem_wq);
}	

void
sem_wait(semaphore_t* sem)
{
	while (1) {
		/*
		 * Try the happy flow first: if there are units left, we are done.
		 */
		register_t state = spinlock_lock_unpremptible(&sem->sem_lock);
		if (sem->sem_count > 0) {
			sem->sem_count--;
			spinlock_unlock_unpremptible(&sem->sem_lock, state);
			return;
		}

		/*
		 * No more units available - as the waitqueue locks itself, we can create a
		 * waiter.
		 */
		spinlock_unlock_unpremptible(&sem->sem_lock, state);
		struct WAITER* w = waitqueue_add(&sem->sem_wq);
		waitqueue_wait(w);
		waitqueue_remove(w);
	}
}

/* vim:set ts=2 sw=2: */
