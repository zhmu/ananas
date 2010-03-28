#include <machine/pcpu.h>
#include <sys/mm.h>
#include <sys/lib.h>
#include <sys/pcpu.h>
#include <sys/thread.h>

void
pcpu_init(struct PCPU* pcpu)
{
	memset(pcpu, 0, sizeof(struct PCPU));

	/*
	 * Initialize the per-CPU idle thread. This does not have to be per-CPU, but
	 * this may come in handy later during stats-collection.
	 */
	thread_init(&pcpu->idlethread);
	md_thread_setidle(&pcpu->idlethread);
	pcpu->idlethread_ptr = &pcpu->idlethread;
}

/* vim:set ts=2 sw=2: */
