/*
 * The reaper is responsible for killing off threads which need to be
 * destroyed - a thread cannot completely exit by itself as it can't
 * free things like its stack.
 *
 * This is commonly used for kernel threads; user threads are generally
 * destroyed by their parent wait()-ing for them.
 */
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/reaper.h"
#include "kernel/result.h"
#include "kernel/thread.h"

namespace {

Spinlock spl_reaper;
ThreadList reaper_list;
Semaphore reaper_sem{0};
Thread reaper_thread;

static void
reaper_reap(void* context)
{
	while(1) {
		reaper_sem.Wait();

		/* Fetch the item to reap from the queue */
		Thread* t;
		{
			SpinlockGuard g(spl_reaper);
			KASSERT(!reaper_list.empty(), "reaper woke up with empty queue?");
			t = &reaper_list.front();
			reaper_list.pop_front();
		}
		t->Deref();
	}
}

Result
start_reaper()
{
	kthread_init(reaper_thread, "reaper", &reaper_reap, NULL);
	reaper_thread.Resume();
	return Result::Success();
}

} // unnamed namespace

void
reaper_enqueue(Thread& t)
{
	{
		SpinlockGuard g(spl_reaper);
		reaper_list.push_back(t);
	}

	reaper_sem.Signal();
}

INIT_FUNCTION(start_reaper, SUBSYSTEM_SCHEDULER, ORDER_MIDDLE);
