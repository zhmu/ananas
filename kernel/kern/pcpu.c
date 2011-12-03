#include <machine/pcpu.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/thread.h>
#include <machine/param.h> /* for PAGE_SIZE */

void
pcpu_init(struct PCPU* pcpu)
{
	/*
	 * Initialize the per-CPU idle thread. This does not have to be per-CPU, but
	 * this may come in handy later during stats-collection.
	 */
	kthread_init(&pcpu->idlethread, &idle_thread, NULL);
	char tmp[64];
	sprintf(tmp, "[idle:cpu%u]", pcpu->cpuid);
	tmp[strlen(tmp) + 1] = '\0'; /* ensure doubly \0 terminated */
	thread_set_args(&pcpu->idlethread, tmp, PAGE_SIZE);
	pcpu->idlethread_ptr = &pcpu->idlethread;

	/*
	 * Mark the idle thread as running; it will never be in a runqueue but this should keep our
	 * invariants safe.
	 */
	pcpu->idlethread.flags &= ~THREAD_FLAG_SUSPENDED;
}

/* vim:set ts=2 sw=2: */
