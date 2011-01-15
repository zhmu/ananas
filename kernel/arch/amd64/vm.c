#include <ananas/types.h>
#include <machine/vm.h>
#include <machine/param.h>
#include <ananas/mm.h>
#include <ananas/lib.h>

#define MEMMASK 0x0fffffff /* HACK */

static int vm_initialized = 0;
extern uint64_t* pml4;
void* bootstrap_get_page();

static void*
vm_get_nextpage()
{
	if (!vm_initialized) {
		return bootstrap_get_page();
	}

	void* ptr = kmalloc(PAGE_SIZE);
	memset(ptr, 0, PAGE_SIZE);
	kprintf("vm_get_nextpage(): malloc said %p\n", ptr);
	return ptr;
}

uint64_t*
vm_get_ptr(addr_t ptr)
{
//	extern void *__entry, *avail;
	return (uint64_t*)(ptr | KERNBASE);
/*
	if(ptr >= (addr_t)&__entry && ptr <= (addr_t)&avail)
		return (uint64_t*)(ptr | KERNBASE);
	return (uint64_t*)ptr;
*/
}

void
vm_mapto_pagedir(uint64_t* pml4, addr_t virt, addr_t phys, size_t num_pages, uint32_t user)
{
	/* XXX we don't yet strip off bits 52-63 yet */
	while(num_pages--) {
		if (pml4[(virt >> 39) & 0x1ff] == 0) {
			pml4[(virt >> 39) & 0x1ff] = (uint64_t)((addr_t)vm_get_nextpage() & MEMMASK) | (user ? PE_US : 0) | PE_RW | PE_P;
		}
		uint64_t* pdpe = (uint64_t*)(vm_get_ptr((addr_t)pml4[(virt >> 39) & 0x1ff] & ~0xfff));

		if (pdpe[(virt >> 30) & 0x1ff] == 0) {
			pdpe[(virt >> 30) & 0x1ff] = (uint64_t)((addr_t)vm_get_nextpage() & MEMMASK) | (user ? PE_US : 0) | PE_RW | PE_P;
		}
		uint64_t* pde = (uint64_t*)vm_get_ptr((addr_t)pdpe[(virt >> 30) & 0x1ff] & ~0xfff);

		if (pde[(virt >> 21) & 0x1ff] == 0) {
			pde[(virt >> 21) & 0x1ff] = (uint64_t)((addr_t)vm_get_nextpage() & MEMMASK) | (user ? PE_US : 0) | PE_RW | PE_P;
		}
		uint64_t* pte = (uint64_t*)vm_get_ptr((addr_t)pde[(virt >> 21) & 0x1ff] & ~0xfff);

		pte[(virt >> 12) & 0x1ff] = (uint64_t)phys | (user ? PE_US : 0) | PE_RW | PE_P;

		/*
		 * Invalidate the address we just added to ensure it'll become active
		 * without delay.
		 */
		__asm __volatile("invlpg %0" : : "m" (*(char*)virt) : "memory");

		virt += PAGE_SIZE; phys += PAGE_SIZE;
	}
}

addr_t
vm_get_phys(uint64_t* pagedir, addr_t addr, int write)
{
	if (!(pagedir[(addr >> 39) & 0x1ff] & PE_P))
		return 0;
	uint64_t* pdpe = (uint64_t*)(vm_get_ptr((addr_t)pagedir[(addr >> 39) & 0x1ff] & ~0xfff));
	if (!(pdpe[(addr >> 30) & 0x1ff] & PE_P))
		return 0;
	uint64_t* pde = (uint64_t*)vm_get_ptr((addr_t)pdpe[(addr >> 30) & 0x1ff] & ~0xfff);
	if (!(pde[(addr >> 21) & 0x1ff] & PE_P))
		return 0;
	uint64_t* pte = (uint64_t*)vm_get_ptr((addr_t)pde[(addr >> 21) & 0x1ff] & ~0xfff);
	uint64_t val = pte[(addr >> 12) & 0x1ff];
	if (!(val & PE_P))
		return 0;
	if (write && !(val & PE_RW))
		return 0;
	return val & ~(PAGE_SIZE - 1);
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
vm_unmap(addr_t addr, size_t num_pages)
{
	/* XXX write me */
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
#ifdef NOTYET
	/* TODO - copy all current pages to new malloc'ed pages */
	vm_initialized = 1;
#endif
}

/* vim:set ts=2 sw=2: */
