#ifndef __LOCK_H__
#define __LOCK_H__

#include <ananas/types.h>
#include <ananas/dqueue.h>

/*
 * Spinlocks are simply locks that just keep the CPU busy waiting if they can't
 * be acquired. They can be used in any context and are designed to be light-
 * weight. They come in a normal and preemptible flavor; the latter will disable
 * interrupts. XXX It's open to debate whether this should always be the case
 *
 * The definition of spinlock_t is in _types/spinlock.h because it's this avoids
 * a circular depency: the waitqueue used spinlocks, but mutexes use the
 * waitqueues, so they can't be declared in this file...
 */

struct SEMAPHORE_WAITER {
	thread_t*		sw_thread;
	int			sw_signalled;
	DQUEUE_FIELDS(struct SEMAPHORE_WAITER);
};
DQUEUE_DEFINE(semaphore_wq, struct SEMAPHORE_WAITER);

/*
 * Semaphores are sleepable locks which guard an amount of units of a
 * particular resource.
 */
typedef struct {
	spinlock_t		sem_lock;
	unsigned int		sem_count;
	struct semaphore_wq	sem_wq;
} semaphore_t;

#define SPINLOCK_DEFAULT_INIT { { 0 } }

/*
 * Mutexes are sleepable locks that will suspend the current thread when the
 * lock is already being held. They cannot be used from interrupt context; they
 * are implemented as binary semaphores.
 */
typedef struct {
	const char*		mtx_name;
	thread_t*		mtx_owner;
	semaphore_t		mtx_sem;
	const char*		mtx_fname;
	int			mtx_line;
} mutex_t;


/* Ordinary spinlocks which can be preempted at any time */
void spinlock_lock(spinlock_t* l);
void spinlock_unlock(spinlock_t* l);
void spinlock_init(spinlock_t* l);

/* Unpremptible spinlocks disable the interrupt flag while they are active */
register_t spinlock_lock_unpremptible(spinlock_t* l);
void spinlock_unlock_unpremptible(spinlock_t* l, register_t state);

/* Mutexes */
void mutex_init(mutex_t* mtx, const char* name);
void mutex_lock_(mutex_t* mtx, const char* file, int line);
void mutex_unlock(mutex_t* mtx);
#define mutex_lock(mtx) mutex_lock_(mtx, __FILE__, __LINE__)
#define MTX_LOCKED 1
#define MTX_UNLOCKED 2
void mutex_assert(mutex_t* mtx, int what);

/* Semaphores */
void sem_init(semaphore_t* sem, int count);
void sem_signal(semaphore_t* sem);
void sem_wait(semaphore_t* sem);
void sem_wait_and_drain(semaphore_t* sem);

#endif /* __LOCK_H__ */
