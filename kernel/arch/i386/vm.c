#include <ananas/types.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <ananas/lib.h>
#include <ananas/mm.h>

extern void* temp_pt_entry;
static int vm_initialized = 0;

/*
 * vm_map_bootstrap() is magic; it will be used when the VM isn't fully
 * initialized yet and can map at most one page safely.
 */
static void
vm_map_bootstrap(addr_t addr, size_t num_pages)
{
	while (num_pages > 0) {
		uint32_t pd_entrynum = addr >> 22;
		if (pagedir[pd_entrynum] == 0) {
			pagedir[pd_entrynum] = ((addr_t)&temp_pt_entry & ~KERNBASE) | PDE_P | PTE_RW;
		}

		uint32_t* pt = (uint32_t*)((pagedir[pd_entrynum] & ~(PAGE_SIZE - 1)) | KERNBASE);
		pt[(((addr >> 12) & ((1 << 10) - 1)))] = addr | PTE_P | PTE_RW;

		num_pages--;
		addr += PAGE_SIZE;
	}
}

addr_t
vm_get_phys(uint32_t* pagedir, addr_t addr, int write)
{
	uint32_t pd_entrynum = addr >> 22;
	if (!(pagedir[pd_entrynum] & PTE_P))
		return 0;
	uint32_t* pt = (uint32_t*)(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1));
	uint32_t val = pt[(((addr >> 12) & ((1 << 10) - 1)))];
	if (!(val & PTE_P))
		return 0;
	if (write && !(val & PTE_RW))
		return 0;
	return val & ~(PAGE_SIZE - 1);
}

void
vm_mapto_pagedir(uint32_t* pagedir, addr_t virt, addr_t phys, size_t num_pages, uint32_t user)
{
	KASSERT(virt % PAGE_SIZE == 0, "unaligned address 0x%x", virt);
	KASSERT(phys % PAGE_SIZE == 0, "unaligned address 0x%x", phys);

	while (num_pages > 0) {
		uint32_t pd_entrynum = virt >> 22;
		if (pagedir[pd_entrynum] == 0) {

			/*
			 * This page isn't yet mapped, so we need to allocate a new page entry
			 * where we can map the pointer.
			 */
			addr_t new_entry = (addr_t)kmalloc(PAGE_SIZE);
			KASSERT((new_entry & (PAGE_SIZE - 1)) == 0, "pagedir entry not page-aligned");
			memset((void*)new_entry, 0, PAGE_SIZE);
			pagedir[pd_entrynum] = new_entry | PDE_P | PTE_RW | (user ? PDE_US : 0);
		}

		uint32_t* pt = (uint32_t*)(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1));
		pt[(((virt >> 12) & ((1 << 10) - 1)))] = phys | PTE_P | PTE_RW | (user ? PTE_US : 0);

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
vm_free_pagedir(uint32_t* pagedir)
{
	for (unsigned int i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
		uint32_t pde = pagedir[i];
		if (pde == 0)
			continue;
		pde &= ~(PAGE_SIZE - 1);
		kfree((void*)pde);
		pagedir[i] = 0;
	}
}

void
vm_map_pagedir(uint32_t* pagedir, addr_t addr, size_t num_pages, uint32_t user)
{
	vm_mapto_pagedir(pagedir, addr, addr, num_pages, user);
}

void
vm_unmap_pagedir(uint32_t* pagedir, addr_t addr, size_t num_pages)
{
	KASSERT(addr % PAGE_SIZE == 0, "unaligned address 0x%x", addr);

	while (num_pages > 0) {
		uint32_t pd_entrynum = addr >> 22;
		if (pagedir[pd_entrynum] == 0) {
			panic("address 0x%x not mapped", addr);
		}

		uint32_t* pt = (uint32_t*)(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1));
		pt[(((addr >> 12) & ((1 << 10) - 1)))] = 0;

		num_pages--;
		addr += PAGE_SIZE;
	}
}

void
vm_map(addr_t addr, size_t num_pages)
{
	if (!vm_initialized)
		return vm_map_bootstrap(addr, num_pages);
	vm_map_pagedir(pagedir, addr, num_pages, 0);
}

void
vm_mapto(addr_t virt, addr_t phys, size_t num_pages)
{
	vm_mapto_pagedir(pagedir, virt, phys, num_pages, 0);
}

void
vm_unmap(addr_t addr, size_t num_pages)
{
	/* Don't care if the VM isn't yet initialized; mappings will vanish anyway */
	if (!vm_initialized)
		return;

	vm_unmap_pagedir(pagedir, addr, num_pages);
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
	/*
	 * This will be called once the first memory zone has been allocated; this
	 * means we can finally truely allocate memory and get rid of the
	 * temp_pt_entry hack...
	 */
	uint32_t j;
	for (j = 0; j < PAGE_SIZE / sizeof(uint32_t); j++) {
		if ((pagedir[j] & ~(PAGE_SIZE - 1)) != ((addr_t)&temp_pt_entry & ~KERNBASE))
			continue;

		/*
		 * OK, we have found the page entry in question. Grab a new
		 * page first, so we can copy the data.
		 */
		void* new_page = kmem_alloc(1);
		KASSERT(new_page != NULL, "got null pointer from kmem_alloc");

		/*
		 * Now, we need to create some mapping to this page; it doesn't matter
		 * where we do it, since this is only temporary (remember that our goal is
		 * to get rid of the temp_pt_entry stuff altogether, so we can add things to
		 * it as we please) - for now, ensure 0x0 is available there XXX
		 */
		*((uint32_t*)&temp_pt_entry) = (addr_t)new_page | PTE_P | PTE_RW;

		/*
		 * Calculate the address of the new page we just mapped.
		 */
		uint32_t* new_page_ptr = (uint32_t*)(j << 22);
		memcpy(new_page_ptr, &temp_pt_entry, PAGE_SIZE);

		/* Get rid of the temporary mapping we just created */
		new_page_ptr[0] = 0;

		/* And create the true mapping */
		new_page_ptr[(addr_t)new_page >> 12] = (addr_t)new_page | PTE_P | PTE_RW;

		/* Finally, we can activate this mapping */
		pagedir[j] = (addr_t)new_page | PTE_P | PTE_RW;
	}

	/*
	 * We are in business and can use vm_map() now. We must reload cr3 to force
	 * the CPU to refresh its cache. XXX This is 486+ only
	 */
	__asm(
		"movl	%%eax, %%cr3\n"
		"jmp	l1\n"
"l1:\n"
	: : "a" (((addr_t)pagedir) & ~KERNBASE));
	vm_initialized = 1;
}

/* vim:set ts=2 sw=2: */
