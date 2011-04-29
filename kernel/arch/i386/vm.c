#include <ananas/types.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/vm.h>

extern uint32_t* kernel_pd;

void
md_map_pages(uint32_t* pagedir, addr_t virt, addr_t phys, size_t num_pages, int flags)
{
	KASSERT(virt % PAGE_SIZE == 0, "unaligned address 0x%x", virt);
	KASSERT(phys % PAGE_SIZE == 0, "unaligned address 0x%x", phys);

#if 0
	if (pagedir != kernel_pd)
		kprintf("md_map_pages(): v=%p, p=%p, flags=0x%x, num_pages=%u, pd=%p\n", virt, phys, flags, num_pages, pagedir);
#endif

	uint32_t pt_flags = 0;
	if (flags & VM_FLAG_READ)
		pt_flags |= PTE_P; /* XXX */
	if (flags & VM_FLAG_USER)
		pt_flags |= PTE_US;
	if (flags & VM_FLAG_WRITE)
		pt_flags |= PTE_RW;

	while (num_pages > 0) {
		uint32_t pd_entrynum = virt >> 22;
		if (pagedir[pd_entrynum] == 0) {
			/*
			 * This page isn't yet mapped, so we need to allocate a new page entry
			 * where we can map the pointer.
			 */
			addr_t new_pt = (addr_t)kmalloc(PAGE_SIZE);
			KASSERT((new_pt & (PAGE_SIZE - 1)) == 0, "pagedir entry not page-aligned");
			memset((void*)new_pt, 0, PAGE_SIZE);
			pagedir[pd_entrynum] = KVTOP(new_pt) | PDE_P | PDE_RW | pt_flags;
		}

		uint32_t* pt = (uint32_t*)(PTOKV(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1)));
		pt[(((virt >> 12) & ((1 << 10) - 1)))] = phys | pt_flags;

		/*
		 * Invalidate the address we just added to ensure it'll become active
		 * without delay.
		 */
		__asm __volatile("invlpg %0" : : "m" (*(char*)virt) : "memory");

		num_pages--;
		virt += PAGE_SIZE; phys += PAGE_SIZE;
	}
}

void
md_unmap_pages(uint32_t* pagedir, addr_t virt, size_t num_pages)
{
	KASSERT(virt % PAGE_SIZE == 0, "unaligned address 0x%x", virt);

	while (num_pages > 0) {
		uint32_t pd_entrynum = virt >> 22;
		if (pagedir[pd_entrynum] & PDE_P) {
			uint32_t* pt = (uint32_t*)PTOKV(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1));
			pt[(((virt >> 12) & ((1 << 10) - 1)))] = 0;
		}

		virt += PAGE_SIZE; num_pages--;
	}
}

addr_t
md_get_mapping(uint32_t* pagedir, addr_t virt, int flags)
{
	uint32_t pd_entrynum = virt >> 22;
	if ((pagedir[pd_entrynum] & PDE_P) == 0)
		return 0;

	uint32_t* pt = (uint32_t*)(PTOKV(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1)));
	uint32_t  pte = pt[(((virt >> 12) & ((1 << 10) - 1)))];
	if ((pte & PTE_P) == 0) /* not present? */
		return 0;
	if ((flags & VM_FLAG_WRITE) && ((pte & PTE_RW) == 0)) /* check writable */
		return 0;
	return pte & ~(PAGE_SIZE - 1);
}

/* Maps physical pages to kernel memory */
void*
vm_map_kernel(addr_t phys, size_t num_pages, int flags)
{
	md_map_pages(kernel_pd, PTOKV(phys), phys, num_pages, flags);
	return (void*)PTOKV(phys);
}

void*
vm_map_device(addr_t phys, size_t len)
{
	len += (PAGE_SIZE - 1);
	len /= PAGE_SIZE;
	return vm_map_kernel(phys, len, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_KERNEL);
}

void
vm_unmap_kernel(addr_t virt, size_t num_pages)
{
	KASSERT(virt >= KERNBASE, "address %x not in kernel range", virt);
	md_unmap_pages(kernel_pd, virt, num_pages);
}

void
vm_free_pagedir(uint32_t* pagedir)
{
	for(unsigned int i = 0; i < VM_FIRST_KERNEL_PT; i++) {
		if (pagedir[i] & PDE_P)
			kfree((void*)PTOKV(pagedir[i] & ~(PAGE_SIZE - 1)));
	}
	kfree(pagedir);
}

void
vm_map_kernel_addr(uint32_t* pd)
{
	for (unsigned int n = 0; n < VM_NUM_KERNEL_PTS; n++) {
		pd[VM_FIRST_KERNEL_PT + n] = kernel_pd[VM_FIRST_KERNEL_PT + n];
	}
}

/* vim:set ts=2 sw=2: */
