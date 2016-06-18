#include <ananas/types.h>
#include <machine/param.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <ananas/lib.h>
#include <ananas/kmem.h>
#include <ananas/page.h>
#include <ananas/thread.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <ananas/vmspace.h>

extern uint32_t* kernel_pd;

void
md_map_pages(vmspace_t* vs, addr_t virt, addr_t phys, size_t num_pages, int flags)
{
	KASSERT(virt % PAGE_SIZE == 0, "unaligned address 0x%x", virt);
	KASSERT(phys % PAGE_SIZE == 0, "unaligned address 0x%x", phys);

	uint32_t pt_flags = 0;
	if (flags & VM_FLAG_READ)
		pt_flags |= PTE_P; /* XXX */
	if (flags & VM_FLAG_USER)
		pt_flags |= PTE_US;
	if (flags & VM_FLAG_WRITE)
		pt_flags |= PTE_RW;
	if (flags & VM_FLAG_DEVICE)
		pt_flags |= PTE_PCD | PDE_PWT;

	uint32_t* pagedir = (vs != NULL) ? vs->vs_md_pagedir : kernel_pd;
	while (num_pages > 0) {
		uint32_t pd_entrynum = virt >> 22;
		if (pagedir[pd_entrynum] == 0) {
			/*
			 * This page isn't yet mapped, so we need to allocate a new page entry
			 * where we can map the pointer.
			 */
			KASSERT(vs != NULL, "unmapped page while mapping kernel pages?");
			struct PAGE* p = page_alloc_single();
			KASSERT(p != NULL, "out of pages");
			DQUEUE_ADD_TAIL(&vs->vs_pages, p);

			/* Map the new page into kernelspace XXX This shouldn't be needed at all; threadspace would work too */
			addr_t new_pt = (addr_t)kmem_map(page_get_paddr(p), PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
			memset((void*)new_pt, 0, PAGE_SIZE);
			pagedir[pd_entrynum] = KVTOP(new_pt) | PDE_P | PDE_RW | pt_flags; /* XXX using pt_flags here is wrong */
		}

		uint32_t* pt = (uint32_t*)(PTOKV(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1)));
		pt[(((virt >> 12) & ((1 << 10) - 1)))] = phys | pt_flags;

		/*
		 * Invalidate the address we just added to ensure it'll become active
		 * without delay XXX This shouldn't be necessary at all
		 */
		__asm __volatile("invlpg %0" : : "m" (*(char*)virt) : "memory");

		num_pages--;
		virt += PAGE_SIZE; phys += PAGE_SIZE;
	}
}

void
md_unmap_pages(vmspace_t* vs, addr_t virt, size_t num_pages)
{
	KASSERT(virt % PAGE_SIZE == 0, "unaligned address 0x%x", virt);

	uint32_t* pagedir = (vs != NULL) ? vs->vs_md_pagedir : kernel_pd;
	while (num_pages > 0) {
		uint32_t pd_entrynum = virt >> 22;
		if (pagedir[pd_entrynum] & PDE_P) {
			uint32_t* pt = (uint32_t*)PTOKV(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1));
			pt[(((virt >> 12) & ((1 << 10) - 1)))] = 0;
		}

		virt += PAGE_SIZE; num_pages--;
	}
}

void
md_kmap(addr_t phys, addr_t virt, size_t num_pages, int flags)
{
	md_map_pages(NULL, virt, phys, num_pages, flags);
}

void
md_kunmap(addr_t virt, size_t num_pages)
{
	md_unmap_pages(NULL, virt, num_pages);
}

void
md_map_kernel(vmspace_t* vs)
{
	uint32_t* pd = vs->vs_md_pagedir;
	for (unsigned int n = 0; n < VM_NUM_KERNEL_PTS; n++) {
		pd[VM_FIRST_KERNEL_PT + n] = kernel_pd[VM_FIRST_KERNEL_PT + n];
	}
}

/* vim:set ts=2 sw=2: */
