#include "i386/thread.h"
#include "lib.h"
#include "thread.h"

extern struct THREAD* threads;

struct THREAD* curthread = NULL; /* XXX make me per-CPU */

void
schedule()
{
	if (curthread == NULL)
		curthread = threads;
	else {
		curthread = curthread->next;
		if (curthread == NULL)
			curthread = threads;
	}

	if (curthread == NULL)
		panic("no threads!");

/*	kprintf("schedule: switching to thread %x\n", curthread);*/
	md_thread_switch(curthread);
	/* NOTREACHED */
}
