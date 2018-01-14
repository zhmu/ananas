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

namespace {

spinlock_t spl_reaper = SPINLOCK_DEFAULT_INIT;
ThreadList reaper_list;
semaphore_t reaper_sem;
Thread reaper_thread;

static void
reaper_reap(void* context)
{
	while(1) {
		sem_wait(&reaper_sem);

		/* Fetch the item to reap from the queue */
		spinlock_lock(&spl_reaper);
		KASSERT(!reaper_list.empty(), "reaper woke up with empty queue?");
		Thread& t = reaper_list.front();
		reaper_list.pop_front();
		spinlock_unlock(&spl_reaper);

		thread_deref(t);
	}
}

errorcode_t
start_reaper()
{
	kthread_init(reaper_thread, "reaper", &reaper_reap, NULL);
	thread_resume(reaper_thread);
	return ananas_success();
}

} // unnamed namespace

void
reaper_enqueue(Thread& t)
{
	spinlock_lock(&spl_reaper);
	reaper_list.push_back(t);
	spinlock_unlock(&spl_reaper);

	sem_signal(&reaper_sem);
}

INIT_FUNCTION(start_reaper, SUBSYSTEM_SCHEDULER, ORDER_MIDDLE);
