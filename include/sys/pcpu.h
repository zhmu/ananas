#include <sys/types.h>
#include <sys/thread.h>
#include <machine/pcpu.h>

#ifndef __PCPU_H__
#define __PCPU_H__

/* Per-CPU information pointer */
struct PCPU {
	MD_PCPU_FIELDS				/* Machine-dependant data */
	uint32_t cpuid;				/* CPU ID */
	void* curthread;			/* current thread */
	void* idlethread_ptr;			/* pointer to idle thread */
	struct THREAD idlethread;		/* idle thread */
};

/* Retrieve the size of the machine-dependant structure */
size_t md_pcpu_get_privdata_length();

/* Introduce a per-cpu structure */
void pcpu_init(struct PCPU* pcpu);

#endif /* __PCPU_H__ */
