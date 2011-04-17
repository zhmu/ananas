#include <ananas/types.h>
#include <ananas/waitqueue.h>
#include <ananas/lock.h>
#include <ananas/schedule.h>
#include <ananas/lib.h>
#include <ananas/mm.h>

#define WAITER_QUEUE_SIZE 100

static struct SPINLOCK spl_freelist;
static struct WAIT_QUEUE w_freelist;

void
waitqueue_init(struct WAIT_QUEUE* wq)
{
	if (wq == NULL) {
		/* Create the freelist with WAITER_QUEUE_SIZE items */
		struct WAITER* w_items = kmalloc(sizeof(struct WAIT_QUEUE) * WAITER_QUEUE_SIZE);
		QUEUE_INIT(&w_freelist);
		for (int i = 0; i < WAITER_QUEUE_SIZE; i++) {
			QUEUE_ADD_TAIL(&w_freelist, w_items);
			w_items++;
		}

		spinlock_init(&spl_freelist);
		return;
	}

	QUEUE_INIT(wq);
	spinlock_init(&wq->wq_lock);
}

void
waitqueue_wait(struct WAIT_QUEUE* wq)
{
	thread_t t = PCPU_GET(curthread);
	if (t == NULL) {
		reschedule();
		return;
	}

	/* Fetch a fresh new waiter */
	spinlock_lock(&spl_freelist);
	KASSERT(!QUEUE_EMPTY(&w_freelist), "out of waiters"); /* XXX deal with this */
	struct WAITER* w = QUEUE_HEAD(&w_freelist);
	QUEUE_POP_HEAD(&w_freelist);
	spinlock_unlock(&spl_freelist);

	/* Append our waiter to the queue */
	w->w_thread = t;
	spinlock_lock(&wq->wq_lock);
	QUEUE_ADD_TAIL(wq, w);
	thread_suspend(t); /* must be done within the lock to ensure we're not signalled right before we wait */
	spinlock_unlock(&wq->wq_lock);

	/* Wait... we'll awake once someone calls waitqueue_signal */
	reschedule();

	/* Once this is reached, we've been woken up */
}

static int
waitqueue_signal_first(struct WAIT_QUEUE* wq)
{
	struct WAITER* w = NULL;

	/* Obtain the first item to signal */
	spinlock_lock(&wq->wq_lock);
	if (!QUEUE_EMPTY(wq)) {
		w = QUEUE_HEAD(wq);
		QUEUE_POP_HEAD(wq);
	}
	spinlock_unlock(&wq->wq_lock);

	if (w == NULL)
		return 0; /* nothing signalled */

	/* Wake up the corresponding thread */
	thread_resume(w->w_thread);

	/* Add the waiter to the free list */
	spinlock_lock(&spl_freelist);
	QUEUE_ADD_TAIL(&w_freelist, w);
	spinlock_unlock(&spl_freelist);
	return 1; /* thread signalled */
}

void
waitqueue_signal(struct WAIT_QUEUE* wq)
{
	waitqueue_signal_first(wq);
}

void
waitqueue_signal_all(struct WAIT_QUEUE* wq)
{
	while (waitqueue_signal_first(wq))
		/* nothing; waitqueue_signal_first() does all the work */ ;
}

/* vim:set ts=2 sw=2: */
