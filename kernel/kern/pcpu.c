#include <machine/pcpu.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/thread.h>

void
pcpu_init(struct PCPU* pcpu)
{
	/*
	 * Initialize the per-CPU idle thread. This does not have to be per-CPU, but
	 * this may come in handy later during stats-collection.
	 */
	thread_init(&pcpu->idlethread, NULL);
	md_thread_setidle(&pcpu->idlethread);
	pcpu->idlethread_ptr = &pcpu->idlethread;
}

/* vim:set ts=2 sw=2: */
