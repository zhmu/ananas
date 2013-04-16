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
	pcpu->nested_irq = 0;

	/*
	 * Hook the idle thread to its specific CPU and set the appropriate priority;
	 * it must only be run as a last-resort.
	 */
	pcpu->idlethread.t_affinity = pcpu->cpuid;
	pcpu->idlethread.t_priority = THREAD_PRIORITY_IDLE;
}

/* vim:set ts=2 sw=2: */
