#ifndef __PCPU_H__
#define __PCPU_H__

#include <ananas/types.h>
#include "kernel/thread.h"
#include "kernel-md/pcpu.h"

/* Per-CPU information pointer */
struct PCPU {
	MD_PCPU_FIELDS				/* Machine-dependant data */
	uint32_t cpuid;				/* CPU ID */
	thread_t* curthread;			/* current thread */
	thread_t* idlethread;			/* idle thread */
	int nested_irq;				/* number of nested IRQ functions */
};

/* Retrieve the size of the machine-dependant structure */
size_t md_pcpu_get_privdata_length();

/* Introduce a per-cpu structure */
void pcpu_init(struct PCPU* pcpu);

/* Get the current thread */
#define PCPU_CURTHREAD() PCPU_GET(curthread)

#endif /* __PCPU_H__ */
