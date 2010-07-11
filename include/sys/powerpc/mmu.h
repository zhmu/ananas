#include <sys/types.h>

#ifndef __POWERPC_MMU_H__
#define __POWERPC_MMU_H__

struct STACKFRAME;

void mmu_map(struct STACKFRAME* sf, uint32_t va, uint32_t pa);

#endif /* __POWERPC_MMU_H__ */
