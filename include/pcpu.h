#include "types.h"

#ifndef __PCPU_H__
#define __PCPU_H__

/* Per-CPU information pointer */
struct PCPU {
	/* Machine-dependant data next */
	MD_PCPU_FIELDS

	/* current thread */
	void* curthread;
};

/* Retrieve the size of the machine-dependant structure */
size_t md_pcpu_get_privdata_length();

/* Introduce a per-cpu structure */
struct PCPU* pcpu_init();

#endif /* __PCPU_H__ */
