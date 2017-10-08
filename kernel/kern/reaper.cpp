/*
 * The reaper is responsible for killing off threads which need to be
 * destroyed - a thread cannot completely exit by itself as it can't
 * free things like its stack.
 *
 * This is commonly used for kernel threads; user threads are generally
 * destroyed by their parent wait()-ing for them.
 */
#include <ananas/error.h>
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/reaper.h"
#include "kernel/thread.h"

static spinlock_t spl_reaper = SPINLOCK_DEFAULT_INIT;
static struct THREAD_QUEUE reaper_queue;
static semaphore_t reaper_sem;
static thread_t reaper_thread;

void
reaper_enqueue(thread_t* t)
{
	spinlock_lock(&spl_reaper);
	LIST_APPEND(&reaper_queue, t);
	spinlock_unlock(&spl_reaper);

	sem_signal(&reaper_sem);
}

static void
reaper_reap(void* context)
{
	while(1) {
		sem_wait(&reaper_sem);

		/* Fetch the item to reap from the queue */
		spinlock_lock(&spl_reaper);
		KASSERT(!LIST_EMPTY(&reaper_queue), "reaper woke up with empty queue?");
		thread_t* t = LIST_HEAD(&reaper_queue);
		LIST_POP_HEAD(&reaper_queue);
		spinlock_unlock(&spl_reaper);

		thread_deref(t);
	}
}

static errorcode_t
start_reaper()
{
	kthread_init(&reaper_thread, "reaper", &reaper_reap, NULL);
	thread_resume(&reaper_thread);
	return ananas_success();
}

INIT_FUNCTION(start_reaper, SUBSYSTEM_SCHEDULER, ORDER_MIDDLE);
