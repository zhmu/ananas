#include "types.h"
#include "machine/vm.h"
#include "lib.h"
#include "param.h"

#define MEMMASK 0x0fffffff /* HACK */

static int vm_initialized = 0;
extern uint64_t* pml4;
void* bootstrap_get_page();

static void*
vm_get_nextpage()
{
	if (!vm_initialized)
		return bootstrap_get_page();

	panic("vm_get_nextpage(): code me");
	return NULL;
}

void
vm_mapto_pagedir(uint64_t* pml4, addr_t virt, addr_t phys, size_t num_pages, uint32_t user)
{
	/* XXX we don't yet strip off bits 52-63 yet */
	while(num_pages--) {
		if (pml4[(virt >> 39) & 0x1ff] == 0) {
			pml4[(virt >> 39) & 0x1ff] = (uint64_t)((addr_t)vm_get_nextpage() & MEMMASK) | (user ? PE_US : 0) | PE_RW | PE_P;
		}
		uint64_t* pdpe = (uint64_t*)(((addr_t)pml4[(virt >> 39) & 0x1ff] & ~0xfff) | KERNBASE);

		if (pdpe[(virt >> 30) & 0x1ff] == 0) {
			pdpe[(virt >> 30) & 0x1ff] = (uint64_t)((addr_t)vm_get_nextpage() & MEMMASK) | (user ? PE_US : 0) | PE_RW | PE_P;
		}
		uint64_t* pde = (uint64_t*)(((addr_t)pdpe[(virt >> 30) & 0x1ff] & ~0xfff) | KERNBASE);

		if (pde[(virt >> 21) & 0x1ff] == 0) {
			pde[(virt >> 21) & 0x1ff] = (uint64_t)((addr_t)vm_get_nextpage() & MEMMASK) | (user ? PE_US : 0) | PE_RW | PE_P;
		}
		uint64_t* pte = (uint64_t*)(((addr_t)pde[(virt >> 21) & 0x1ff] & ~0xfff) | KERNBASE);

		pte[(virt >> 12) & 0x1ff] = (uint64_t)phys | (user ? PE_US : 0) | PE_RW | PE_P;

		virt += PAGE_SIZE; phys += PAGE_SIZE;
	}
}

void
vm_map_pagedir(uint64_t* pml4, addr_t addr, size_t num_pages, uint32_t user)
{
	vm_mapto_pagedir(pml4, addr, addr, num_pages, user);
}

void
vm_map(addr_t addr, size_t num_pages)
{
	vm_mapto_pagedir(pml4, addr, addr, num_pages, 0);
}

void
vm_mapto(addr_t virt, addr_t phys, size_t num_pages)
{
	vm_mapto_pagedir(pml4, virt, phys, num_pages, 0);
}

void*
vm_map_device(addr_t addr, size_t len)
{
	len = (len + PAGE_SIZE - 1) / PAGE_SIZE;
	vm_map(addr, len);
	return (void*)addr;
}

void
vm_init()
{
}

/* vim:set ts=2 sw=2: */
