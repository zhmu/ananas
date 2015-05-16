#ifndef __RELOCATE_H__
#define __RELOCATE_H__

#include "types.h"

struct RELOCATE_INFO {
	/* Virtual address start/end of relocation entry */
	uint64_t ri_vaddr_start, ri_vaddr_end;
	/* Physical address start/end of relocation entry */
	uint64_t ri_paddr_start, ri_paddr_end;
	/* Entry point */
	uint64_t ri_entry;
};

void relocate_kernel(struct RELOCATE_INFO* ri);

#endif /* __RELOCATE_H__ */
