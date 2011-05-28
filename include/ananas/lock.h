#ifndef __LOCK_H__
#define __LOCK_H__

#include <machine/atomic.h>
#include <ananas/_types/spinlock.h>
#include <ananas/waitqueue.h>
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

/*
 * Mutexes are sleepable locks that will suspend the current thread when the
 * lock is already being held. They cannot be used from interrupt context.
 */
typedef struct {
	const char*		mtx_name;
	thread_t*		mtx_owner;
	atomic_t		mtx_var;
	struct WAIT_QUEUE	mtx_wq;
} mutex_t;

#define SPINLOCK_DEFAULT_INIT { { 0 } }

/* Ordinary spinlocks which can be preempted at any time */
void spinlock_lock(spinlock_t* l);
void spinlock_unlock(spinlock_t* l);
void spinlock_init(spinlock_t* l);

/* Unpremptible spinlocks disable the interrupt flag while they are active */
register_t spinlock_lock_unpremptible(spinlock_t* l);
void spinlock_unlock_unpremptible(spinlock_t* l, register_t state);

/* Mutexes */
void mutex_init(mutex_t* mtx, const char* name);
void mutex_lock(mutex_t* mtx);
void mutex_unlock(mutex_t* mtx);

#endif /* __LOCK_H__ */
