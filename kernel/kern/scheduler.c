#include <machine/thread.h>
#include <ananas/pcpu.h>
#include <ananas/lib.h>
#include <ananas/lock.h>
#include <ananas/pcpu.h>
#include <ananas/schedule.h>
#include <ananas/thread.h>

extern struct THREAD* threads;
static int scheduler_active = 0;

struct SPINLOCK spl_scheduler = { 0 };

void
schedule()
{
	struct THREAD* curthread;
	struct THREAD* newthread;

	spinlock_lock(&spl_scheduler);
	newthread = NULL;

	curthread = PCPU_GET(curthread);
	if (scheduler_active) {
		if (curthread != NULL) {
			/* We have a current thread; remove the active flag */
			curthread->flags &= ~THREAD_FLAG_ACTIVE;

			/* Start with the next thread, if any */
			newthread = curthread->next;
		}

		/* If we cannot pick a next thread, use the first */
		if (newthread == NULL)
			 newthread = threads;

		/* Scan for the next thread to run */
		while (newthread->flags & (THREAD_FLAG_ACTIVE | THREAD_FLAG_SUSPENDED)) {
			newthread = newthread->next;
			if (newthread == curthread)
				break;
			if (newthread == NULL)
				newthread = threads;
			if (newthread == curthread)
				break;
		}

		/*
		 * OK, scanning was done. If newthread is identical to curthread
		 * and we cannot schedule curthread, it means no new thread can be
		 * found and as such, we must activate the idle thread.
		 */
		if (newthread != curthread) {
			/* This new thread will do */
			newthread->flags |= THREAD_FLAG_ACTIVE;

			/*
			 * Safety checks. Note that we must skip this check in the idlethread case,
			 * because it will always be marked as suspended. This prevents the scheduler
			 * from picking it up, yet we can see if in the process list.
			 */
			KASSERT((newthread->flags & THREAD_FLAG_SUSPENDED) == 0, "schedule: activating non-idle thread %p", newthread);
		} else {
			newthread = PCPU_GET(idlethread_ptr);
		}
	} else {
		/*
		 * Scheduler is not active; schedule our current thread if we can, do the
		 * idle thread otherwise. Note that we do _not_ update current thread, this
		 * is a hack... XXX
		 */
		newthread = (curthread->flags & THREAD_FLAG_SUSPENDED) ? PCPU_GET(idlethread_ptr) : curthread;
		spinlock_unlock(&spl_scheduler);
		md_thread_switch(newthread, curthread);
		/* NOTREACHED */
	}

	PCPU_SET(curthread, newthread);
	spinlock_unlock(&spl_scheduler);
	if (newthread != curthread && 0)
		kprintf("schedule: CPU %u: switching %x to %x\n", PCPU_GET(cpuid), curthread, newthread);

	md_thread_switch(newthread, curthread);
	/* NOTREACHED */
}

void
scheduler_switchto(thread_t newthread)
{
	spinlock_lock(&spl_scheduler);
	thread_t curthread = PCPU_GET(curthread);
	if (curthread != NULL) {
		/* We have a current thread; remove the active flag */
		curthread->flags &= ~THREAD_FLAG_ACTIVE;
	}

	newthread->flags |= THREAD_FLAG_ACTIVE;
	PCPU_SET(curthread, newthread);
	spinlock_unlock(&spl_scheduler);

	/* all set - ask the md-code to run the thread */
	md_thread_switch(newthread, curthread);
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
	struct THREAD* curthread = PCPU_GET(curthread);
	if (curthread != NULL)
		curthread->flags &= ~THREAD_FLAG_ACTIVE;

	/* Activate our idle thread; the timer interrupt will steal the context away */
	struct THREAD* newthread = PCPU_GET(idlethread_ptr);
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

/* vim:set ts=2 sw=2: */
