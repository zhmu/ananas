/*
 * This contains the scheduler; it has two queues: a runqueue (containing all
 * threads that can run) and a sleepqueue (threads which cannot run). The
 * current thread is never on either of these queues; the reason is that the
 * administration of threads is distinct from the scheduler, and having the
 * scheduler re-add a thread that has expired its timeslice back to the
 * runqueue avoids nasty races (as well as being much easier to follow)
 *
 * Finally, the idle thread is neither in the runqueue nor in the sleepqueue;
 * it will be chosen when nothing else is available.
 */
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
static struct SCHEDULER_QUEUE sched_runqueue;
static struct SCHEDULER_QUEUE sched_sleepqueue;

void
scheduler_init_thread(thread_t* t)
{
	t->t_sched_priv.sp_thread = t;

	/* Hook the thread to our sleepqueue */
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	DQUEUE_ADD_TAIL(&sched_sleepqueue, &t->t_sched_priv);
	spinlock_unlock_unpremptible(&spl_scheduler, state);
}

void
scheduler_cleanup_thread(thread_t* t)
{
	/*
	 * We need to remove the thread from the sleepqueue; it cannot be on the
	 * running queue as the thread must be a zombie once this is called.
	 */
	KASSERT(THREAD_IS_ZOMBIE(t), "cleaning up non-zombie thread");
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	DQUEUE_REMOVE(&sched_sleepqueue, &t->t_sched_priv);
	spinlock_unlock_unpremptible(&spl_scheduler, state);
}

void
scheduler_add_thread(thread_t* t)
{
	KASSERT(!THREAD_IS_SUSPENDED(t) == 0, "adding suspend thread %p", t);
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	/* Remove the thread from the sleepqueue ... */
	DQUEUE_REMOVE(&sched_sleepqueue, &t->t_sched_priv);
	/* ... and add it to the runqueue */
	DQUEUE_ADD_TAIL(&sched_runqueue, &t->t_sched_priv);
	spinlock_unlock_unpremptible(&spl_scheduler, state);
}

void
scheduler_remove_thread(thread_t* t)
{
	KASSERT(THREAD_IS_SUSPENDED(t), "removing non-suspended thread %p", t);
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	/* Remove the thread from the runqueue ... */
	DQUEUE_REMOVE(&sched_runqueue, &t->t_sched_priv);
	/* ... and add it to the sleepqueue */
	DQUEUE_ADD_TAIL(&sched_sleepqueue, &t->t_sched_priv);
	spinlock_unlock_unpremptible(&spl_scheduler, state);
}

void
schedule()
{
	thread_t* curthread = PCPU_GET(curthread);
	thread_t* idle_thread = PCPU_GET(idlethread_ptr);

	/* Cancel any rescheduling as we are about to schedule here */
	if (curthread != NULL)
		curthread->t_flags &= ~THREAD_FLAG_RESCHEDULE;

	/* Pick the next thread to schedule and add it to the back of the queue */
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	KASSERT(state, "irq's must be enabled at this point");
	struct SCHED_PRIV* next_sched;
	if (!DQUEUE_EMPTY(&sched_runqueue)) {
		next_sched = DQUEUE_HEAD(&sched_runqueue);
		DQUEUE_POP_HEAD(&sched_runqueue);
	} else {
		/* If there was no thread, revert to idle */
		next_sched = &idle_thread->t_sched_priv;
	}

	/* Sanity checks */
	thread_t* newthread = next_sched->sp_thread;
	KASSERT(!THREAD_IS_SUSPENDED(newthread), "activating suspended thread %p", newthread);
	if (curthread != NULL) {
		/*
		 * If the current thread is not suspended, this means it got interrupted
		 * involuntary and must be placed back on the running queue; otherwise it
		 * must have been placed on the runqueue already.
		 *
		 * We must also take care not to re-add zombie threads; these cannot be
		 * run. The same goes for the idle-thread as it will be chosen if the
		 * runqueue is empty - it makes no sense to schedule idle-time.
		 */
		if (!THREAD_IS_SUSPENDED(curthread) && !THREAD_IS_ZOMBIE(curthread) &&
		   curthread != idle_thread)
			DQUEUE_ADD_TAIL(&sched_runqueue, &curthread->t_sched_priv);
		curthread->t_flags &= ~THREAD_FLAG_ACTIVE;
	}

	/* Schedule our new thread - this will enable interrupts as required */
	newthread->t_flags |= THREAD_FLAG_ACTIVE;
	PCPU_SET(curthread, newthread);

	/* Now unlock the scheduler lock but do _not_ enable interrupts */
	spinlock_unlock(&spl_scheduler);

	if (curthread != newthread) {
		md_thread_switch(newthread, curthread);
	}

	/* Re-enable interrupts as they were */
	md_interrupts_enable();
}

static errorcode_t
scheduler_launch()
{
	thread_t* idlethread = PCPU_GET(idlethread_ptr);

	/*
	 * Activate the idle threadit; the MD startup code should have done the
	 * appropriate code/stack switching bits. All we need to do is set up the
	 * scheduler enough so that it accepts our idle thread.
	 */
	md_interrupts_disable();
	PCPU_SET(curthread, idlethread);
	idlethread->t_flags |= THREAD_FLAG_ACTIVE;

	/* Run it */
	scheduler_active++;

	/*
	 * Enable the interrupt flag and become the idle thread (which we already are
	 * at this point, just not idling)
	 */
	md_interrupts_enable();
	idle_thread();

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
	kprintf("runqueue\n");
	if (!DQUEUE_EMPTY(&sched_runqueue)) {
		DQUEUE_FOREACH(&sched_runqueue, s, struct SCHED_PRIV) {
			kprintf("  thread %p\n", s->sp_thread);
		}
	} else {
		kprintf("(empty)\n");
	}
	kprintf("sleepqueue\n");
	if (!DQUEUE_EMPTY(&sched_sleepqueue)) {
		DQUEUE_FOREACH(&sched_sleepqueue, s, struct SCHED_PRIV) {
			kprintf("  thread %p\n", s->sp_thread);
		}
	} else {
		kprintf("(empty)\n");
	}
}
#endif /* OPTION_KDB */

/* vim:set ts=2 sw=2: */
