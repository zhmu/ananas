#include <machine/thread.h>
#include <machine/interrupts.h>
#include <ananas/error.h>
#include <ananas/pcpu.h>
#include <ananas/pcpu.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/init.h>
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

	/* Cancel any rescheduling as we are about to schedule here */
	if (curthread != NULL)
		curthread->flags &= ~THREAD_FLAG_RESCHEDULE;

	/* Pick the next thread to schedule and add it to the back of the queue */
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	KASSERT(state, "irq's must be enabled at this point");
	struct SCHED_PRIV* next_sched = NULL;
	if (!DQUEUE_EMPTY(&sched_queue)) {
		next_sched = DQUEUE_HEAD(&sched_queue);
		DQUEUE_POP_HEAD(&sched_queue);
		DQUEUE_ADD_TAIL(&sched_queue, next_sched);
	}
	/* Now unlock the scheduler lock but do _not_ enable interrupts */
	spinlock_unlock(&spl_scheduler);

	/* If there was no thread or the thread is already running, revert to idle */
	if (next_sched == NULL || (next_sched->sp_thread->flags & THREAD_FLAG_ACTIVE)) {
		next_sched = &((thread_t*)PCPU_GET(idlethread_ptr))->sched_priv;
	}

	/* Sanity checks */
	thread_t* newthread = next_sched->sp_thread;
	KASSERT((newthread->flags & THREAD_FLAG_SUSPENDED) == 0, "activating suspended thread %p", newthread);
	if (curthread != NULL)
		curthread->flags &= ~THREAD_FLAG_ACTIVE;

	/* Schedule our new thread - this will enable interrupts as required */
	newthread->flags |= THREAD_FLAG_ACTIVE;
	PCPU_SET(curthread, newthread);
	if (curthread != newthread)
		md_thread_switch(newthread, curthread);

	/* Re-enable interrupts as they were */
	md_interrupts_enable();
}

static errorcode_t
scheduler_launch()
{
	thread_t* idlethread = PCPU_GET(idlethread_ptr);

	/*
	 * Activate the idle thread and bootstrap it; the bootstrap code will do the
	 * appropriate code/stack switching bits. All we need to do is set up the
	 * scheduler enough so that it accepts our idle thread.
	 */
	md_interrupts_disable();
	PCPU_SET(curthread, idlethread);
	idlethread->flags |= THREAD_FLAG_ACTIVE;

	/* Run it */
	scheduler_active++;
	md_thread_bootstrap(idlethread);

	/* NOTREACHED */
	return ANANAS_ERROR_OK;
}

INIT_FUNCTION(scheduler_launch, SUBSYSTEM_SCHEDULER, ORDER_LAST);

void
scheduler_activate()
{
	scheduler_active++;
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

#ifdef OPTION_KDB
void
kdb_cmd_scheduler(int num_args, char** arg)
{
	if (!DQUEUE_EMPTY(&sched_queue))
		DQUEUE_FOREACH(&sched_queue, s, struct SCHED_PRIV) {
			kprintf("thread %p\n", s->sp_thread);
		}
}
#endif /* OPTION_KDB */

/* vim:set ts=2 sw=2: */
