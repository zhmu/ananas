/*
 * The reaper is responsible for killing off threads which need to be
 * destroyed - a thread cannot completely exit by itself as it can't
 * free things like its stack.
 *
 * This is commonly used for kernel threads; user threads are generally
 * destroyed by their parent wait()-ing for them.
 */
#include <ananas/reaper.h>
#include <ananas/error.h>
#include <ananas/init.h>
#include <ananas/lib.h>
#include <ananas/thread.h>

static spinlock_t spl_reaper = SPINLOCK_DEFAULT_INIT;
static struct THREAD_QUEUE reaper_queue;
static semaphore_t reaper_sem;
static thread_t reaper_thread;

void
reaper_enqueue(thread_t* t)
{
	spinlock_lock(&spl_reaper);
	DQUEUE_ADD_TAIL(&reaper_queue, t);
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
		KASSER(!DQUEUE_EMPTY(&reaper_queue), "reaper woke up with empty queue?");
		thread_t* t = DQUEUE_HEAD(&reaper_queue);
		DQUEUE_POP_HEAD(&reaper_queue);
		spinlock_unlock(&spl_reaper);

		thread_deref(t);
	}
}

static errorcode_t
start_reaper()
{
	kthread_init(&reaper_thread, &reaper_reap, NULL);
	thread_set_args(&reaper_thread, "[reaper]\0", 10);
	thread_resume(&reaper_thread);
	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(start_reaper, SUBSYSTEM_SCHEDULER, ORDER_MIDDLE);
