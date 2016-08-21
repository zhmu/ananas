#include <machine/pcpu.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/thread.h>
#include <machine/param.h> /* for PAGE_SIZE */

void
pcpu_init(struct PCPU* pcpu)
{
	pcpu->idlethread = kmalloc(sizeof(struct THREAD));
	KASSERT(pcpu->idlethread != NULL, "out of memory for idle thread");

	char name[64];
	snprintf(name, sizeof(name) - 1, "idle:cpu%u", pcpu->cpuid);
	name[sizeof(name) - 1] = '\0';
	kthread_init(pcpu->idlethread, name, &idle_thread, NULL);
	pcpu->nested_irq = 0;

	/*
	 * Hook the idle thread to its specific CPU and set the appropriate priority;
	 * it must only be run as a last-resort.
	 */
	pcpu->idlethread->t_affinity = pcpu->cpuid;
	pcpu->idlethread->t_priority = THREAD_PRIORITY_IDLE;
}

/* vim:set ts=2 sw=2: */
