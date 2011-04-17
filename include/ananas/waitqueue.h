#ifndef __WAITQUEUE_H__
#define __WAITQUEUE_H__

#include <ananas/types.h>
#include <ananas/thread.h>
#include <ananas/lock.h>
#include <ananas/pcpu.h>
#include <ananas/queue.h>

/*
 * A waiter is a thread waiting on some event; these are implemented by having
 * a wait queue for the given event. Wait queues are pre-allocated and are
 * designed to be light-weight.
 */
struct WAITER {
	thread_t	w_thread;		/* waiting thread */
	QUEUE_FIELDS(struct WAITER);
};

QUEUE_DEFINE_BEGIN(WAIT_QUEUE, struct WAITER)
	struct SPINLOCK	wq_lock;
QUEUE_DEFINE_END

/* Initialize a given wait queue; if wq is NULL, initializes the freelist */
void waitqueue_init(struct WAIT_QUEUE* wq);

/* Adds the current thread to a waitqueue and blocks until it signals */
void waitqueue_wait(struct WAIT_QUEUE* wq);

/* Wakes the first waiter of wait queue wq */
void waitqueue_signal(struct WAIT_QUEUE* wq);

/* Wakes all waiters of wait queue wq */
void waitqueue_signal_all(struct WAIT_QUEUE* wq);

#endif /* __WAITQUEUE_H__ */
