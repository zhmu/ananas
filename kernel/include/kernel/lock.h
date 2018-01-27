#ifndef __LOCK_H__
#define __LOCK_H__

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel-md/atomic.h"

struct Thread;

struct Spinlock final {
	atomic_t		sl_var;
};

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

struct SemaphoreWaiter final : util::List<SemaphoreWaiter>::NodePtr {
	Thread*			sw_thread;
	int			sw_signalled;
};

typedef util::List<SemaphoreWaiter> SemaphoreWaiterList;

/*
 * Semaphores are sleepable locks which guard an amount of units of a
 * particular resource.
 */
struct Semaphore final {
	Spinlock		sem_lock;
	unsigned int		sem_count;
	SemaphoreWaiterList	sem_wq;
};

/*
 * Mutexes are sleepable locks that will suspend the current thread when the
 * lock is already being held. They cannot be used from interrupt context; they
 * are implemented as binary semaphores.
 */
struct Mutex final {
	const char*		mtx_name;
	Thread*			mtx_owner;
	Semaphore		mtx_sem;
	const char*		mtx_fname;
	int			mtx_line;
};

typedef struct MUTEX mutex_t;


/* Ordinary spinlocks which can be preempted at any time */
void spinlock_lock(Spinlock& l);
void spinlock_unlock(Spinlock& l);
void spinlock_init(Spinlock& l);

/* Unpremptible spinlocks disable the interrupt flag while they are active */
register_t spinlock_lock_unpremptible(Spinlock& l);
void spinlock_unlock_unpremptible(Spinlock& l, register_t state);

/* Mutexes */
void mutex_init(Mutex& mtx, const char* name);
void mutex_lock_(Mutex& mtx, const char* file, int line);
void mutex_unlock(Mutex& mtx);
#define mutex_lock(mtx) mutex_lock_(mtx, __FILE__, __LINE__)
#define MTX_LOCKED 1
#define MTX_UNLOCKED 2
void mutex_assert(Mutex& mtx, int what);
int mutex_trylock_(Mutex& mtx, const char* fname, int len);
#define mutex_trylock(mtx) mutex_trylock_(mtx, __FILE__, __LINE__)

/* Semaphores */
void sem_init(Semaphore& sem, int count);
void sem_signal(Semaphore& sem);
void sem_wait(Semaphore& sem);
int sem_trywait(Semaphore& sem);
void sem_wait_and_drain(Semaphore& sem);

#endif /* __LOCK_H__ */
