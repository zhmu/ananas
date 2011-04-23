#ifndef __WAITQUEUE_H__
#define __WAITQUEUE_H__

#include <ananas/types.h>
#include <ananas/thread.h>
#include <ananas/lock.h>
#include <ananas/pcpu.h>
#include <ananas/dqueue.h>

/*
 * A waiter is a thread waiting on some event; these are implemented by having
 * a wait queue for the given event. Wait queues are pre-allocated and are
 * designed to be light-weight.
 */
struct WAITER {
	struct SPINLOCK	w_lock;			/* spinlock protecting the waiter */
	thread_t	w_thread;		/* waiting thread */
	int		w_signalled;		/* number of times signalled */
	struct WAIT_QUEUE* w_wq;		/* owner queue */
	DQUEUE_FIELDS(struct WAITER);
};

DQUEUE_DEFINE_BEGIN(WAIT_QUEUE, struct WAITER)
	struct SPINLOCK	wq_lock;
DQUEUE_DEFINE_END

/* Initialize a given wait queue; if wq is NULL, initializes the freelist */
void waitqueue_init(struct WAIT_QUEUE* wq);

/* Allocate a waiter slot for the current thread in the specified waitqueue */
struct WAITER* waitqueue_add(struct WAIT_QUEUE* wq);

/* Removes the current thread's waiter slot */
void waitqueue_remove(struct WAITER* w);

/* Wait until the given waiter is released signals */
void waitqueue_wait(struct WAITER* w);

/* Wakes all waiters of the given waitqueue */
void waitqueue_signal(struct WAIT_QUEUE* wq);

/* Resets the given waiter to unsignalled state */
void waitqueue_reset_waiter(struct WAITER* w);

#endif /* __WAITQUEUE_H__ */
