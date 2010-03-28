#include <machine/thread.h>
#include <sys/pcpu.h>
#include <sys/lib.h>
#include <sys/lock.h>
#include <sys/pcpu.h>
#include <sys/thread.h>

extern struct THREAD* threads;

struct SPINLOCK spl_scheduler = { 0 };

void
schedule()
{
	struct THREAD* curthread;
	struct THREAD* newthread;

	spinlock_lock(&spl_scheduler);
	curthread = PCPU_GET(curthread);

	newthread = curthread;
	if (newthread == NULL) {
		newthread = threads;
		if (newthread == NULL)
			goto no_thread;
	}

	while (newthread->flags & (THREAD_FLAG_ACTIVE | THREAD_FLAG_SUSPENDED)) {
		newthread = newthread->next;
		if (newthread == curthread)
			break;
		if (newthread == NULL)
			newthread = threads;
		if (newthread == curthread)
			break;
	}

	if (curthread != NULL)
		curthread->flags &= ~THREAD_FLAG_ACTIVE;
	if (newthread != NULL)
		newthread->flags |= THREAD_FLAG_ACTIVE;

	PCPU_SET(curthread, newthread);
	spinlock_unlock(&spl_scheduler);

	if (newthread == NULL) {
no_thread:
		/* No (usuable) thread; pick the idle thread */
		newthread = PCPU_GET(idlethread_ptr);
	}

	if (newthread != curthread && 0)
		kprintf("schedule: CPU %u: switching %x to %x\n", PCPU_GET(cpuid), curthread, newthread);

	md_thread_switch(newthread, curthread);
	/* NOTREACHED */
}

/* vim:set ts=2 sw=2: */
