#include <ananas/types.h>
#include <machine/vm.h>
#include <machine/param.h>
#include <machine/macro.h>
#include <machine/mmu.h>
#include <ananas/mm.h>
#include <ananas/thread.h>
#include <ananas/lib.h>

extern struct STACKFRAME bsp_sf;

void*
vm_map_kernel(addr_t addr, size_t num_pages)
{
	vm_mapto(addr, addr, num_pages);
	return (void*)addr;
}

void
vm_mapto(addr_t virt, addr_t phys, size_t num_pages)
{
	while (num_pages--) {
		uint32_t vsid = bsp_sf.sf_sr[virt >> 28];
		mmu_map(&bsp_sf, virt, phys);
		/*
		 * We reload the segment register if the mapping changed the appropriate
		 * segment... maybe this is too crude?
		 */
		if (bsp_sf.sf_sr[virt >> 28] != vsid) {
			mtsrin(virt, bsp_sf.sf_sr[virt >> 28]);
			__asm __volatile("sync");
		}
		virt += PAGE_SIZE;
		phys += PAGE_SIZE;
	}
}

void
vm_unmap(addr_t addr, size_t num_pages)
{
	while (num_pages--) {
		mmu_unmap(&bsp_sf, addr);
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
