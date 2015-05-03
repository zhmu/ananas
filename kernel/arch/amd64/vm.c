#include <ananas/types.h>
#include <machine/vm.h>
#include <machine/param.h>
#include <ananas/mm.h>
#include <ananas/kmem.h>
#include <ananas/lib.h>
#include <ananas/thread.h>
#include <ananas/vm.h>

extern uint64_t* kernel_pagedir;

static addr_t
vm_get_nextpage(thread_t* t, uint64_t page_flags)
{
	KASSERT(t != NULL, "unmapped page while mapping kernel pages?");
	struct PAGE* p = page_alloc_single();
	KASSERT(p != NULL, "out of pages");

	/*
	 * If the page isn't mapped globally, it belongs to a thread and we should
	 * administer it there so we can free it once the thread is freed.
	 */
	if ((page_flags & PE_C_G) == 0)
		DQUEUE_ADD_TAIL(&t->t_pages, p);

	/* Map this page in kernel-space XXX How do we clean it up? */
	addr_t phys = page_get_paddr(p);
	void* va = kmem_map(phys, PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
	memset(va, 0, PAGE_SIZE);
	return phys | page_flags;
}

static inline uint64_t*
pt_resolve_addr(uint64_t entry)
{
#define ADDR_MASK 0xffffffffff000 /* bits 12 .. 51 */
	return (uint64_t*)(KMEM_DIRECT_VA_START + (entry & ADDR_MASK));
}

void
md_map_pages(thread_t* t, addr_t virt, addr_t phys, size_t num_pages, int flags)
{
	/* Flags for the mapped pages themselves */
	uint64_t pt_flags = 0;
	if (flags & VM_FLAG_READ)
		pt_flags |= PE_P;	/* XXX */
	if (flags & VM_FLAG_USER)
		pt_flags |= PE_US;
	if (flags & VM_FLAG_WRITE)
		pt_flags |= PE_RW;
	if (flags & VM_FLAG_DEVICE)
		pt_flags |= PE_PCD | PE_PWT;
#if 0
	if ((flags & VM_FLAG_EXECUTE) == 0)
		pt_flags |= PE_NX;
#endif

	/* Flags for the page-directory leading up to the mapped page */
	uint64_t pd_flags = PE_US | PE_P | PE_RW;

	/* XXX we don't yet strip off bits 52-63 yet */
	uint64_t* pagedir = (t != NULL) ? t->md_pagedir : kernel_pagedir;
	while(num_pages--) {
		if (pagedir[(virt >> 39) & 0x1ff] == 0) {
			pagedir[(virt >> 39) & 0x1ff] = vm_get_nextpage(t, pd_flags);
		}

		/*
		 * XXX We only look at the top level pagetable flags to determine whether
		 *     the page should be mapped globally - the idea is that all ranges
		 *     (KVA, kernel) where this should happen are pre-allocated in startup.c
		 *     and thus thee is no need to look further...
		 */
		if (pagedir[(virt >> 39) & 0x1ff] & PE_C_G) {
			pd_flags |= PE_C_G;
			pt_flags |= PE_G;
		}

		uint64_t* pdpe = pt_resolve_addr(pagedir[(virt >> 39) & 0x1ff]);
		if (pdpe[(virt >> 30) & 0x1ff] == 0) {
			pdpe[(virt >> 30) & 0x1ff] = vm_get_nextpage(t, pd_flags);
		}

		uint64_t* pde = pt_resolve_addr(pdpe[(virt >> 30) & 0x1ff]);
		if (pde[(virt >> 21) & 0x1ff] == 0) {
			pde[(virt >> 21) & 0x1ff] = vm_get_nextpage(t, pd_flags);
		}

		uint64_t* pte = pt_resolve_addr(pde[(virt >> 21) & 0x1ff]);
		pte[(virt >> 12) & 0x1ff] = (uint64_t)phys | pt_flags;

		virt += PAGE_SIZE; phys += PAGE_SIZE;
	}
}

int
md_get_mapping(thread_t* t, addr_t virt, int flags, addr_t* phys)
{
	uint64_t* pagedir = (t != NULL) ? t->md_pagedir : kernel_pagedir;
	if (!(pagedir[(virt >> 39) & 0x1ff] & PE_P))
		return 0;
	uint64_t* pdpe = pt_resolve_addr(pagedir[(virt >> 39) & 0x1ff]);
	if (!(pdpe[(virt >> 30) & 0x1ff] & PE_P))
		return 0;
	uint64_t* pde = pt_resolve_addr(pdpe[(virt >> 30) & 0x1ff]);
	if (!(pde[(virt >> 21) & 0x1ff] & PE_P))
		return 0;
	uint64_t* pte = pt_resolve_addr(pde[(virt >> 21) & 0x1ff]);
	uint64_t val = pte[(virt >> 12) & 0x1ff];
	if (!(val & PE_P))
		return 0;
	if ((flags & VM_FLAG_WRITE) && !(val & PE_RW))
		return 0;
	if (phys != NULL)
		*phys = val & ~(PAGE_SIZE - 1);
	return 1;
}

void
md_unmap_pages(thread_t* t, addr_t virt, size_t num_pages)
{
	/* XXX we don't yet strip off bits 52-63 yet */
	uint64_t* pagedir = (t != NULL) ? t->md_pagedir : kernel_pagedir;
	while(num_pages--) {
		if (pagedir[(virt >> 39) & 0x1ff] == 0) {
			panic("t=%p, virt=%p -> l1 not mapped (%p)", t, virt, pagedir[(virt >> 39) & 0x1ff]);
		}

		uint64_t* pdpe = pt_resolve_addr(pagedir[(virt >> 39) & 0x1ff]);
		if (pdpe[(virt >> 30) & 0x1ff] == 0) {
			panic("t=%p, virt=%p -> l2 not mapped (%p)", t, virt, pagedir[(virt >> 30) & 0x1ff]);
		}

		uint64_t* pde = pt_resolve_addr(pdpe[(virt >> 30) & 0x1ff]);
		if (pde[(virt >> 21) & 0x1ff] == 0) {
			panic("t=%p, virt=%p -> l3 not mapped (%p)", t, virt, pagedir[(virt >> 21) & 0x1ff]);
		}

		/* XXX perhaps we should check if this is actually mapped */
		uint64_t* pte = pt_resolve_addr(pde[(virt >> 21) & 0x1ff]);
		int global = (pte[(virt >> 12) & 0x1ff] & PE_G);
		pte[(virt >> 12) & 0x1ff] = 0;
		if (global) {
			/*
			 * We just unmapped a global virtual address; this means we'll have to
			 * explicitely invalidate it.
			 */
			__asm __volatile("invlpg %0" : : "m" (virt) : "memory");
		}
		virt += PAGE_SIZE;
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
vm_init()
{
}

void
md_map_kernel(thread_t* t)
{
	/*
	 * We can just copy the entire kernel pagemap over; it's shared with everything else.
	 */
	memcpy(t->md_pagedir, kernel_pagedir, PAGE_SIZE);
}

/* vim:set ts=2 sw=2: */
