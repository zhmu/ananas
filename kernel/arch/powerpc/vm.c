#include <sys/types.h>
#include <machine/vm.h>
#include <machine/param.h>
#include <machine/macro.h>
#include <machine/mmu.h>
#include <sys/mm.h>
#include <sys/thread.h>
#include <sys/lib.h>

extern struct STACKFRAME bsp_sf;

void
vm_map(addr_t addr, size_t num_pages)
{
	while (num_pages--) {
		uint32_t vsid = bsp_sf.sf_sr[addr >> 28];
		mmu_map(&bsp_sf, addr, addr);
		/*
		 * We reload the segment register if the mapping changed the appropriate
		 * segment... maybe this is too crude?
		 */
		if (bsp_sf.sf_sr[addr >> 28] != vsid) {
			mtsrin(addr, bsp_sf.sf_sr[addr >> 28]);
			__asm __volatile("sync");
		}
		addr += PAGE_SIZE;
	}
}

void*
vm_map_device(addr_t addr, size_t len)
{
	panic("TODO: vm_map_device(): addr=%x, len=%x\n", addr, len);
	return NULL;
}

/* vim:set ts=2 sw=2: */
