#include <ananas/types.h>

#ifndef __POWERPC_MMU_H__
#define __POWERPC_MMU_H__

struct STACKFRAME;
extern struct STACKFRAME bsp_sf;

void mmu_init();
void mmu_map(struct STACKFRAME* sf, uint32_t va, uint32_t pa);
int mmu_unmap(struct STACKFRAME* sf, uint32_t va);
void mmu_map_kernel(struct STACKFRAME* sf);

#endif /* __POWERPC_MMU_H__ */
