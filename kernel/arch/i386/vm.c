#include "i386/types.h"
#include "i386/vm.h"
#include "lib.h"
#include "mm.h"
#include "param.h"

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

		uint32_t* pt = (uint32_t*)(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1) | KERNBASE);
		pt[(((addr >> 12) & ((1 << 10) - 1)))] = addr | PTE_P | PTE_RW;

		num_pages--;
		addr += PAGE_SIZE;
	}
}

void
vm_map(addr_t addr, size_t num_pages)
{
	KASSERT(addr % PAGE_SIZE == 0, "unaligned address 0x%x", addr);

	if (!vm_initialized)
		return vm_map_bootstrap(addr, num_pages);

	while (num_pages > 0) {
		uint32_t pd_entrynum = addr >> 22;
		if (pagedir[pd_entrynum] == 0) {

			/*
			 * This page isn't yet mapped, so we need to allocate a new page entry
			 * where we can map the pointer.
			 */
			addr_t new_entry = (addr_t)kmalloc(PAGE_SIZE);
			memset((void*)new_entry, 0, PAGE_SIZE);
			pagedir[pd_entrynum] = new_entry | PDE_P | PTE_RW;
		}

		uint32_t* pt = (uint32_t*)(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1));
		pt[(((addr >> 12) & ((1 << 10) - 1)))] = addr | PTE_P | PTE_RW;

		num_pages--;
		addr += PAGE_SIZE;
	}
}

void
vm_unmap(addr_t addr, size_t num_pages)
{
	KASSERT(addr % PAGE_SIZE == 0, "unaligned address 0x%x", addr);

	/* Don't care if the VM isn't yet initialized; mappings will vanish anyway */
	if (!vm_initialized)
		return;

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
		void* new_page = kmem_alloc(PAGE_SIZE);
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
	 * the CPU to refresh its cache.
	 */
	__asm(
		"movl	%%eax, %%cr3\n"
		"jmp	l1\n"
"l1:\n"
	: : "a" (((addr_t)pagedir) & ~KERNBASE));
	vm_initialized = 1;
}

/* vim:set ts=2 sw=2: */
