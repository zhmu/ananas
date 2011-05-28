#include <machine/thread.h>
#include <ananas/pcpu.h>
#include <ananas/lib.h>
#include <ananas/lock.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/thread.h>
#include "options.h"

static int scheduler_active = 0;

static spinlock_t spl_scheduler = SPINLOCK_DEFAULT_INIT;
static struct SCHEDULER_QUEUE sched_queue;

void
scheduler_init_thread(thread_t* t)
{
	t->sched_priv.sp_thread = t;
}

void
scheduler_add_thread(thread_t* t)
{
	KASSERT((t->flags & THREAD_FLAG_SUSPENDED) == 0, "scheduling suspend thread %p", t);
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	/* First of all, see if it's already in the list - this is a bug */
	if (!DQUEUE_EMPTY(&sched_queue))
		DQUEUE_FOREACH(&sched_queue, s, struct SCHED_PRIV) {
			KASSERT(s->sp_thread != t, "thread %p already in queue", t);
		}
	DQUEUE_ADD_TAIL(&sched_queue, &t->sched_priv);
	spinlock_unlock_unpremptible(&spl_scheduler, state);
}

void
scheduler_remove_thread(thread_t* t)
{
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	if (!DQUEUE_EMPTY(&sched_queue))
		DQUEUE_FOREACH(&sched_queue, s, struct SCHED_PRIV) {
			if (s->sp_thread == t) {
				DQUEUE_REMOVE(&sched_queue, s);
				spinlock_unlock_unpremptible(&spl_scheduler, state);
				return;
			}
		}
	spinlock_unlock_unpremptible(&spl_scheduler, state);
	panic("thread %p not in queue", t);
}

void
schedule()
{
	thread_t* curthread = PCPU_GET(curthread);

	/* Pick the next thread to schedule and add it to the back of the queue */
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	struct SCHED_PRIV* next_sched = NULL;
	if (!DQUEUE_EMPTY(&sched_queue)) {
		next_sched = DQUEUE_HEAD(&sched_queue);
		DQUEUE_POP_HEAD(&sched_queue);
		DQUEUE_ADD_TAIL(&sched_queue, next_sched);
	}
	spinlock_unlock_unpremptible(&spl_scheduler, state);

	/* If there was no thread or the thread is already running, revert to idle */
	if (next_sched == NULL || (next_sched->sp_thread->flags & THREAD_FLAG_ACTIVE)) {
		next_sched = &((thread_t*)PCPU_GET(idlethread_ptr))->sched_priv;
	}

	/* Sanity checks */
	thread_t* newthread = next_sched->sp_thread;
	KASSERT((newthread->flags & THREAD_FLAG_SUSPENDED) == 0, "activating suspended thread %p", newthread);

	PCPU_SET(curthread, newthread);
	if (newthread != curthread && 0) {
		kprintf("schedule: CPU %u: switching %x to %x\n", PCPU_GET(cpuid), curthread, newthread);
	}

	if (curthread != NULL) /* happens if the thread exited */
		curthread->flags &= ~THREAD_FLAG_ACTIVE;
	newthread->flags |=  THREAD_FLAG_ACTIVE;
	md_thread_switch(newthread, curthread);
	/* NOTREACHED */
}

void
scheduler_activate()
{
	if (scheduler_active++ > 0)
		return;

	/*
	 * Remove the 'active' flag of our current thread; this will cause it to be
	 * rescheduled if necessary.
	 */
	thread_t* curthread = PCPU_GET(curthread);
	if (curthread != NULL)
		curthread->flags &= ~THREAD_FLAG_ACTIVE;

	/* Activate our idle thread; the timer interrupt will steal the context away */
	thread_t* newthread = PCPU_GET(idlethread_ptr);
	PCPU_SET(curthread, newthread);
	md_thread_switch(newthread, curthread);
	/* NOTREACHED */
}

void
scheduler_deactivate()
{
	scheduler_active--;
}

int
scheduler_activated()
{
	return scheduler_active;
}

#ifdef KDB
void
kdb_cmd_scheduler(int num_args, char** arg)
{
	if (!DQUEUE_EMPTY(&sched_queue))
		DQUEUE_FOREACH(&sched_queue, s, struct SCHED_PRIV) {
			kprintf("thread %p\n", s->sp_thread);
		}
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
