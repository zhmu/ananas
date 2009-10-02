#include "i386/thread.h"
#include "lib.h"
#include "thread.h"

extern struct THREAD* threads;

struct THREAD* curthread = NULL; /* XXX make me per-CPU */

void
schedule()
{
	struct THREAD* newthread = curthread;

	if (newthread == NULL)
		newthread = threads;
	else {
		newthread = newthread->next;
		if (newthread == NULL)
			newthread = threads;
	}

	if (newthread == NULL)
		panic("no threads!");

#if 0
	if (newthread != curthread)
		kprintf("schedule: switching to thread %x\n", newthread);
#endif
	curthread = newthread;
	md_thread_switch(curthread);
	/* NOTREACHED */
}
