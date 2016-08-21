/*
 * This contains the scheduler; it has two queues: a runqueue (containing all
 * threads that can run) and a sleepqueue (threads which cannot run). The
 * current thread is never on either of these queues; the reason is that the
 * administration of threads is distinct from the scheduler, and having the
 * scheduler re-add a thread that has expired its timeslice back to the
 * runqueue avoids nasty races (as well as being much easier to follow)
 */
#include <machine/thread.h>
#include <machine/interrupts.h>
#include <ananas/error.h>
#include <ananas/kdb.h>
#include <ananas/pcpu.h>
#include <ananas/pcpu.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/init.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/thread.h>
#include "options.h"

#include <machine/vm.h>
#include <machine/param.h>

/*
 * The next define adds specific debugger assertions that may incur a
 * significant performance penalty - these assertions are named
 * SCHED_ASSERT.
 */
#define DEBUG_SCHEDULER
#define SCHED_KPRINTF(...)

static int scheduler_active = 0;

static spinlock_t spl_scheduler = SPINLOCK_DEFAULT_INIT;
static struct SCHEDULER_QUEUE sched_runqueue;
static struct SCHEDULER_QUEUE sched_sleepqueue;

#ifdef DEBUG_SCHEDULER
static int
scheduler_is_on_queue(struct SCHEDULER_QUEUE* q, thread_t* t)
{
	int n = 0;
	DQUEUE_FOREACH(q, s, struct SCHED_PRIV) {
		if (s->sp_thread == t)
			n++;
	}
	return n;
}
#define SCHED_ASSERT(x,...) KASSERT((x), __VA_ARGS__)
#else
#define SCHED_ASSERT(x,...)
#endif

void
scheduler_init_thread(thread_t* t)
{
	/* Hook up our private scheduling entity */
	t->t_sched_priv.sp_thread = t;

	/* Mark the thread as suspened - the scheduler is responsible for this */
	t->t_flags |= THREAD_FLAG_SUSPENDED;

	/* Hook the thread to our sleepqueue */
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	KASSERT(scheduler_is_on_queue(&sched_runqueue, t) == 0, "new thread is already on runq?");
	KASSERT(scheduler_is_on_queue(&sched_sleepqueue, t) == 0, "new thread is already on sleepq?");
	DQUEUE_ADD_TAIL(&sched_sleepqueue, &t->t_sched_priv);
	spinlock_unlock_unpremptible(&spl_scheduler, state);
}

static void
scheduler_add_thread_locked(thread_t* t)
{
	KASSERT(scheduler_is_on_queue(&sched_runqueue, t) == 0, "adding thread on runq?");
	KASSERT(scheduler_is_on_queue(&sched_sleepqueue, t) == 0, "adding thread on sleepq?");

	/*
	 * Add it to the runqueue - note that we must preserve order here
	 * because the threads must be sorted in order of priority
	 * XXX Note that this is O(n) - we can do better
	 */
	int inserted = 0;
	if (!DQUEUE_EMPTY(&sched_runqueue)) {
		DQUEUE_FOREACH(&sched_runqueue, s, struct SCHED_PRIV) {
			KASSERT(s->sp_thread != t, "thread %p already in runqueue", t);
			if (s->sp_thread->t_priority <= t->t_priority)
				continue;

			/* Found a thread with a lower priority; we can insert it here */
			DQUEUE_INSERT_BEFORE(&sched_runqueue, s, &t->t_sched_priv);
			inserted++;
			break;
		}
	}
	if (!inserted)
		DQUEUE_ADD_TAIL(&sched_runqueue, &t->t_sched_priv);
}

void
scheduler_add_thread(thread_t* t)
{
	SCHED_KPRINTF("%s: t=%p\n", __func__, t);
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	KASSERT(THREAD_IS_SUSPENDED(t), "adding non-suspended thread %p", t);
	SCHED_ASSERT(scheduler_is_on_queue(&sched_runqueue, t) == 0, "adding thread %p already on runqueue", t);
	SCHED_ASSERT(scheduler_is_on_queue(&sched_sleepqueue, t) == 1, "adding thread %p not on sleepqueue", t);
	/* Remove the thread from the sleepqueue ... */
	DQUEUE_REMOVE(&sched_sleepqueue, &t->t_sched_priv);
	/* ... and add it to the runqueue ... */
	scheduler_add_thread_locked(t);
	/*
	 * ... and finally, update the flags: we must do this in the scheduler lock because
	 *     no one else is allowed to touch the thread while we're moving it
	 */
	t->t_flags &= ~THREAD_FLAG_SUSPENDED;
	spinlock_unlock_unpremptible(&spl_scheduler, state);
}

void
scheduler_remove_thread(thread_t* t)
{
	SCHED_KPRINTF("%s: t=%p\n", __func__, t);
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);
	KASSERT(!THREAD_IS_SUSPENDED(t), "removing suspended thread %p", t);
	SCHED_ASSERT(scheduler_is_on_queue(&sched_sleepqueue, t) == 0, "removing thread already on sleepqueue");
	SCHED_ASSERT(scheduler_is_on_queue(&sched_runqueue, t) == 1, "removing thread not on runqueue");
	/* Remove the thread from the runqueue ... */
	DQUEUE_REMOVE(&sched_runqueue, &t->t_sched_priv);
	/* ... add it to the sleepqueue ... */
	DQUEUE_ADD_TAIL(&sched_sleepqueue, &t->t_sched_priv);
	/*
	 * ... and finally, update the flags: we must do this in the scheduler lock because
	 *     no one else is allowed to touch the thread while we're moving it
	 */
	t->t_flags |= THREAD_FLAG_SUSPENDED;
	spinlock_unlock_unpremptible(&spl_scheduler, state);
}

void
scheduler_exit_thread(thread_t* t)
{
	/*
	 * Note that interrupts must be disabled - this is important because we are about to
	 * remove the thread from the schedulers runqueue, and it will not be re-added again.
	 * Thus, if a context switch would occur, the final exiting code will not be run.
	 */
	spinlock_lock_unpremptible(&spl_scheduler);
	SCHED_ASSERT(scheduler_is_on_queue(&sched_runqueue, t) == 1, "exiting thread already not on sleepqueue");
	SCHED_ASSERT(scheduler_is_on_queue(&sched_sleepqueue, t) == 0, "exiting thread on runqueue");
	/* Thread seems sane; remove it from the runqueue */
	DQUEUE_REMOVE(&sched_runqueue, &t->t_sched_priv);
	/*
	 * Turn the thread into a zombie; we'll soon be letting go of the scheduler lock, but all
	 * resources are gone and the thread can be destroyed from now on - interrupts are disabled,
	 * so we'll be certain to clear the active flag. A thread which is an inactive zombie won't be
	 * scheduled anymore because it's on neither runqueue nor sleepqueue; the scheduler won't know
	 * about the thread at all.
	 */
	t->t_flags |= THREAD_FLAG_ZOMBIE;
	/* Let go of the scheduler lock but leave interrupts disabled */
	spinlock_unlock(&spl_scheduler);

	/* Force a reschedule - won't return */
	schedule();
}

void
scheduler_release(thread_t* old)
{
	/* Release the old thread; it is now safe to schedule it elsewhere */
	SCHED_KPRINTF("old[%p] -active\n", old);
	old->t_flags &= ~THREAD_FLAG_ACTIVE;
}

void
schedule()
{
	thread_t* curthread = PCPU_GET(curthread);
	int cpuid = PCPU_GET(cpuid);
	KASSERT(curthread != NULL, "no current thread active");
	SCHED_KPRINTF("schedule(): cpu=%u curthread=%p\n", cpuid, curthread);

	/*
	 * Grab the scheduler lock and disable interrupts; note that they need not be
	 * enabled - this happens in interrupt context, which needs to clean up
	 * before another interrupt can be handled.
	 */
	register_t state = spinlock_lock_unpremptible(&spl_scheduler);

	/* Cancel any rescheduling as we are about to schedule here */
	curthread->t_flags &= ~THREAD_FLAG_RESCHEDULE;

	/* Pick the next thread to schedule */
	KASSERT(!DQUEUE_EMPTY(&sched_runqueue), "runqueue cannot be empty");
	struct SCHED_PRIV* next_sched = NULL;
	DQUEUE_FOREACH(&sched_runqueue, sp, struct SCHED_PRIV) {
		/* Skip the thread if we can't schedule it here */
		if (sp->sp_thread->t_affinity != THREAD_AFFINITY_ANY &&
			  sp->sp_thread->t_affinity != cpuid)
			continue;
		if (THREAD_IS_ACTIVE(sp->sp_thread) && sp->sp_thread != curthread)
			continue;
		next_sched = sp;
		break;
	}
	KASSERT(next_sched != NULL, "nothing on the runqueue for cpu %u", cpuid);

	/* Sanity checks */
	thread_t* newthread = next_sched->sp_thread;
	KASSERT(!THREAD_IS_SUSPENDED(newthread), "activating suspended thread %p", newthread);
	KASSERT(newthread == curthread || !THREAD_IS_ACTIVE(newthread), "activating active thread %p", newthread);
	SCHED_ASSERT(scheduler_is_on_queue(&sched_runqueue, newthread) == 1, "scheduling thread not on runqueue (?)");
	SCHED_ASSERT(scheduler_is_on_queue(&sched_sleepqueue, newthread) == 0, "scheduling thread on sleepqueue");

	SCHED_KPRINTF("%s[%d]: newthread=%p curthread=%p\n", __func__, cpuid, newthread, curthread);

	/*
	 * If the current thread is not suspended, this means it got interrupted
	 * involuntary and must be placed back on the running queue; otherwise it
	 * must have been placed on the runqueue already. We'll add it to the back,
	 * in order to obtain round-robin scheduling within each priority level.
	 *
	 * We must also take care not to re-add zombie threads; these must not be
	 * re-added to either scheduler queue.
	 */
	if (!THREAD_IS_SUSPENDED(curthread) && !THREAD_IS_ZOMBIE(curthread)) {
		SCHED_KPRINTF("%s[%d]: removing t=%p from runqueue\n", __func__, cpuid, curthread);
		DQUEUE_REMOVE(&sched_runqueue, &curthread->t_sched_priv);
		SCHED_KPRINTF("%s[%d]: re-adding t=%p\n", __func__, cpuid, curthread);
		scheduler_add_thread_locked(curthread);
	}

	/*
	 * Schedule our new thread; by marking it as active, it will not be picked up by another
	 * CPU.
	 */
	newthread->t_flags |= THREAD_FLAG_ACTIVE;
	PCPU_SET(curthread, newthread);

	/* Now unlock the scheduler lock but do _not_ enable interrupts */
	spinlock_unlock(&spl_scheduler);

	if (curthread != newthread) {
		thread_t* prev = md_thread_switch(newthread, curthread);
		scheduler_release(prev);
	}

	/* Re-enable interrupts if they were */
	md_interrupts_restore(state);
}

void
scheduler_launch()
{
	thread_t* idlethread = PCPU_GET(idlethread);
	KASSERT(PCPU_GET(curthread) == idlethread, "idle thread not correct");

	/*
	 * Activate the idle thread; the MD startup code should have done the
	 * appropriate code/stack switching bits. All we need to do is set up the
	 * scheduler enough so that it accepts our idle thread.
	 */
	md_interrupts_disable();
	PCPU_SET(curthread, idlethread);

	/* Run it */
	scheduler_active++;

	md_interrupts_enable();
}

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
KDB_COMMAND(scheduler, NULL, "Display scheduler status")
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
